#include "custom_fs.h"
#include <linux/lz4.h>
#include <linux/zstd.h>
#include <linux/quota.h>
#include <linux/jbd2.h>
#include <linux/crc32.h>

/* Journal Implementation */
static struct customfs_journal_header *journal_header;
static struct journal_s *journal;

int customfs_journal_start(struct super_block *sb) {
    struct customfs_fs_info *fsi = CUSTOMFS_SB(sb);
    int err;

    journal = jbd2_journal_init_dev(sb->s_bdev, sb->s_bdev,
                                  fsi->sb->first_data_block,
                                  CUSTOMFS_JOURNAL_BLOCKS, 1024);
    if (!journal)
        return -EINVAL;

    err = jbd2_journal_load(journal);
    if (err) {
        jbd2_journal_destroy(journal);
        return err;
    }

    return 0;
}

int customfs_journal_commit(struct super_block *sb) {
    handle_t *handle;
    int err;

    handle = jbd2_journal_start(journal, CUSTOMFS_TX_MAX_BLOCKS);
    if (IS_ERR(handle))
        return PTR_ERR(handle);

    err = jbd2_journal_get_write_access(handle, sb->s_bdev,
                                      journal_header->start_block);
    if (err)
        goto out;

    journal_header->sequence++;
    journal_header->checksum = crc32(0, journal_header,
                                   sizeof(*journal_header) - 4);

    err = jbd2_journal_dirty_metadata(handle, sb->s_bdev,
                                    journal_header->start_block);
out:
    jbd2_journal_stop(handle);
    return err;
}

/* Compression Implementation */
int customfs_compress_data(struct inode *inode, void *data,
                         size_t len, void **out, size_t *out_len) {
    struct customfs_inode_info *ci = CUSTOMFS_I(inode);
    void *workspace;
    int ret;

    switch (ci->c_info.algorithm) {
        case CUSTOMFS_COMPRESS_LZ4:
            workspace = kmalloc(LZ4_MEM_COMPRESS, GFP_KERNEL);
            if (!workspace)
                return -ENOMEM;

            *out = kmalloc(LZ4_compressBound(len), GFP_KERNEL);
            if (!*out) {
                kfree(workspace);
                return -ENOMEM;
            }

            *out_len = LZ4_compress_default(data, *out, len,
                                          LZ4_compressBound(len),
                                          workspace);
            kfree(workspace);
            break;

        case CUSTOMFS_COMPRESS_ZSTD:
            *out = kmalloc(ZSTD_compressBound(len), GFP_KERNEL);
            if (!*out)
                return -ENOMEM;

            *out_len = ZSTD_compress(*out, ZSTD_compressBound(len),
                                   data, len, ci->c_info.level);
            break;

        default:
            return -EINVAL;
    }

    if (*out_len <= 0) {
        kfree(*out);
        return -EIO;
    }

    return 0;
}

/* Extended Attributes Implementation */
int customfs_get_xattr(struct inode *inode, const char *name,
                      void *buffer, size_t size) {
    struct buffer_head *bh;
    struct customfs_xattr_entry *entry;
    size_t name_len = strlen(name);
    int ret = -ENODATA;

    if (!inode->i_op->xattr_get)
        return -EOPNOTSUPP;

    bh = sb_bread(inode->i_sb, CUSTOMFS_I(inode)->xattr_block);
    if (!bh)
        return -EIO;

    entry = (struct customfs_xattr_entry *)bh->b_data;
    while ((char *)entry < bh->b_data + inode->i_sb->s_blocksize) {
        if (entry->name_len == name_len &&
            memcmp(entry->name, name, name_len) == 0) {
            if (buffer && size < entry->value_len) {
                ret = -ERANGE;
                goto out;
            }
            if (buffer)
                memcpy(buffer, entry->name + entry->name_len,
                       entry->value_len);
            ret = entry->value_len;
            goto out;
        }
        entry = (void *)entry + sizeof(*entry) + entry->name_len +
                ((entry->value_len + 3) & ~3);
    }

out:
    brelse(bh);
    return ret;
}

/* Quota Implementation */
int customfs_quota_check(struct inode *inode, size_t blocks) {
    struct customfs_inode_info *ci = CUSTOMFS_I(inode);
    struct customfs_quota_info *qi = &ci->q_info;

    if (qi->block_usage + blocks > qi->block_limit)
        return -EDQUOT;

    return 0;
}

/* Block Allocation Strategy Implementation */
int customfs_alloc_blocks_strategy(struct super_block *sb, int strategy,
                                 unsigned long goal, unsigned long len) {
    struct customfs_fs_info *fsi = CUSTOMFS_SB(sb);
    struct customfs_block_group *bg;
    unsigned long block = 0;
    int group, ret = -ENOSPC;

    mutex_lock(&fsi->lock);

    switch (strategy) {
        case ALLOC_BEST_FIT:
            /* Find smallest sufficient free chunk */
            for (group = 0; group < fsi->sb->groups_count; group++) {
                bg = &fsi->groups[group];
                if (bg->free_blocks >= len) {
                    unsigned long tmp = find_next_zero_bit(
                        (unsigned long *)bg->block_bitmap,
                        fsi->sb->blocks_per_group, 0);
                    if (tmp + len <= fsi->sb->blocks_per_group) {
                        block = group * fsi->sb->blocks_per_group + tmp;
                        ret = 0;
                        break;
                    }
                }
            }
            break;

        case ALLOC_NEXT_FIT:
            /* Start from last allocated block */
            group = goal / fsi->sb->blocks_per_group;
            do {
                bg = &fsi->groups[group];
                if (bg->free_blocks >= len) {
                    unsigned long start = (group == goal / fsi->sb->blocks_per_group) ?
                                       goal % fsi->sb->blocks_per_group : 0;
                    block = find_next_zero_bit(
                        (unsigned long *)bg->block_bitmap,
                        fsi->sb->blocks_per_group, start);
                    if (block + len <= fsi->sb->blocks_per_group) {
                        block += group * fsi->sb->blocks_per_group;
                        ret = 0;
                        break;
                    }
                }
                group = (group + 1) % fsi->sb->groups_count;
            } while (group != goal / fsi->sb->blocks_per_group);
            break;

        default: /* ALLOC_FIRST_FIT */
            for (group = 0; group < fsi->sb->groups_count; group++) {
                bg = &fsi->groups[group];
                if (bg->free_blocks >= len) {
                    block = find_next_zero_bit(
                        (unsigned long *)bg->block_bitmap,
                        fsi->sb->blocks_per_group, 0);
                    if (block + len <= fsi->sb->blocks_per_group) {
                        block += group * fsi->sb->blocks_per_group;
                        ret = 0;
                        break;
                    }
                }
            }
    }

    if (ret == 0) {
        /* Mark blocks as used */
        for (unsigned long i = 0; i < len; i++)
            set_bit(block + i, (unsigned long *)fsi->groups[group].block_bitmap);
        bg->free_blocks -= len;
        fsi->sb->free_blocks -= len;
        mark_buffer_dirty(fsi->group_desc[group]);
    }

    mutex_unlock(&fsi->lock);
    return ret == 0 ? block : ret;
}

/* Block Group Management */
int customfs_init_block_groups(struct super_block *sb) {
    struct customfs_fs_info *fsi = CUSTOMFS_SB(sb);
    struct customfs_block_group *bg;
    int i, err = 0;

    fsi->groups = kmalloc_array(fsi->sb->groups_count,
                               sizeof(struct customfs_block_group),
                               GFP_KERNEL);
    if (!fsi->groups)
        return -ENOMEM;

    fsi->group_desc = kmalloc_array(fsi->sb->groups_count,
                                   sizeof(struct buffer_head *),
                                   GFP_KERNEL);
    if (!fsi->group_desc) {
        err = -ENOMEM;
        goto out_free_groups;
    }

    /* Read block group descriptors */
    for (i = 0; i < fsi->sb->groups_count; i++) {
        fsi->group_desc[i] = sb_bread(sb, fsi->sb->first_group_desc + i);
        if (!fsi->group_desc[i]) {
            err = -EIO;
            goto out_free_group_desc;
        }
        bg = (struct customfs_block_group *)fsi->group_desc[i]->b_data;
        memcpy(&fsi->groups[i], bg, sizeof(*bg));
    }

    return 0;

out_free_group_desc:
    while (--i >= 0)
        brelse(fsi->group_desc[i]);
    kfree(fsi->group_desc);
out_free_groups:
    kfree(fsi->groups);
    return err;
}

/* Module Init/Exit */
static int __init init_customfs_advanced(void) {
    printk(KERN_INFO "CustomFS: Loading advanced features\n");
    return 0;
}

static void __exit exit_customfs_advanced(void) {
    printk(KERN_INFO "CustomFS: Unloading advanced features\n");
}

module_init(init_customfs_advanced);
module_exit(exit_customfs_advanced);
