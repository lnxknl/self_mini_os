#include "custom_fs.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/statfs.h>
#include <linux/writeback.h>
#include <linux/quotaops.h>
#include <linux/seq_file.h>
#include <linux/parser.h>
#include <linux/uio.h>

/* Module Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Custom FS Developer");
MODULE_DESCRIPTION("Custom File System Implementation");
MODULE_VERSION("1.0");

/* Superblock Operations */
static struct super_operations customfs_super_ops = {
    .alloc_inode    = customfs_alloc_inode,
    .destroy_inode  = customfs_destroy_inode,
    .write_inode    = customfs_write_inode,
    .evict_inode    = customfs_evict_inode,
    .put_super      = customfs_put_super,
    .sync_fs        = customfs_sync_fs,
    .statfs         = customfs_statfs,
};

/* File Operations */
static struct file_operations customfs_file_ops = {
    .llseek         = generic_file_llseek,
    .read_iter      = generic_file_read_iter,
    .write_iter     = generic_file_write_iter,
    .mmap           = generic_file_mmap,
    .fsync          = generic_file_fsync,
    .splice_read    = generic_file_splice_read,
    .splice_write   = iter_file_splice_write,
};

/* Directory Operations */
static struct file_operations customfs_dir_ops = {
    .owner          = THIS_MODULE,
    .iterate_shared = customfs_readdir,
    .read           = generic_read_dir,
    .fsync          = generic_file_fsync,
};

/* Inode Operations */
static struct inode_operations customfs_inode_ops = {
    .create         = customfs_create,
    .lookup         = customfs_lookup,
    .link           = customfs_link,
    .unlink         = customfs_unlink,
    .symlink        = customfs_symlink,
    .mkdir          = customfs_mkdir,
    .rmdir          = customfs_rmdir,
    .rename         = customfs_rename,
    .getattr        = customfs_getattr,
    .setattr        = customfs_setattr,
};

/* Superblock Functions */
int customfs_fill_super(struct super_block *sb, void *data, int silent) {
    struct customfs_fs_info *fsi;
    struct inode *root_inode;
    int ret = 0;

    // Allocate filesystem info structure
    fsi = kzalloc(sizeof(struct customfs_fs_info), GFP_KERNEL);
    if (!fsi)
        return -ENOMEM;

    // Initialize filesystem info
    mutex_init(&fsi->lock);
    sb->s_fs_info = fsi;
    sb->s_magic = CUSTOMFS_MAGIC;
    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_op = &customfs_super_ops;

    // Read superblock from disk
    fsi->sb_bh = sb_bread(sb, 0);
    if (!fsi->sb_bh) {
        ret = -EIO;
        goto out_free_fsi;
    }

    fsi->sb = (struct customfs_super_block *)fsi->sb_bh->b_data;
    
    // Verify filesystem magic number
    if (le32_to_cpu(fsi->sb->magic) != CUSTOMFS_MAGIC) {
        if (!silent)
            printk(KERN_ERR "customfs: wrong magic number\n");
        ret = -EINVAL;
        goto out_free_bh;
    }

    // Create root inode
    root_inode = customfs_new_inode(sb, S_IFDIR | 0755);
    if (IS_ERR(root_inode)) {
        ret = PTR_ERR(root_inode);
        goto out_free_bh;
    }

    root_inode->i_ino = CUSTOMFS_ROOT_INO;
    root_inode->i_op = &customfs_inode_ops;
    root_inode->i_fop = &customfs_dir_ops;
    root_inode->i_mode = S_IFDIR | 0755;
    
    // Create root dentry
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        ret = -ENOMEM;
        goto out_free_root_inode;
    }

    return 0;

out_free_root_inode:
    iput(root_inode);
out_free_bh:
    brelse(fsi->sb_bh);
out_free_fsi:
    kfree(fsi);
    return ret;
}

/* Inode Management Functions */
struct inode *customfs_new_inode(struct super_block *sb, umode_t mode) {
    struct customfs_inode_info *ci;
    struct inode *inode;
    int err;

    inode = new_inode(sb);
    if (!inode)
        return ERR_PTR(-ENOMEM);

    ci = CUSTOMFS_I(inode);
    
    // Initialize inode
    inode_init_owner(inode, NULL, mode);
    inode->i_blocks = 0;
    inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
    
    // Set operations based on inode type
    if (S_ISDIR(mode)) {
        inode->i_op = &customfs_inode_ops;
        inode->i_fop = &customfs_dir_ops;
    } else if (S_ISREG(mode)) {
        inode->i_op = &customfs_inode_ops;
        inode->i_fop = &customfs_file_ops;
    }

    // Allocate inode on disk
    err = customfs_alloc_inode_block(sb);
    if (err < 0) {
        make_bad_inode(inode);
        iput(inode);
        return ERR_PTR(err);
    }

    return inode;
}

/* File Operations Implementation */
int customfs_get_block(struct inode *inode, sector_t block,
                      struct buffer_head *bh, int create) {
    struct super_block *sb = inode->i_sb;
    struct customfs_inode_info *ci = CUSTOMFS_I(inode);
    int err = 0;
    
    // Handle direct blocks
    if (block < 12) {
        if (create && !ci->direct_blocks[block]) {
            err = customfs_alloc_block(sb);
            if (err < 0)
                return err;
            ci->direct_blocks[block] = err;
            mark_inode_dirty(inode);
        }
        map_bh(bh, sb, ci->direct_blocks[block]);
        return 0;
    }
    
    // Handle indirect blocks (simplified)
    return -EIO;  // For now, only support direct blocks
}

/* Directory Operations Implementation */
int customfs_readdir(struct file *file, struct dir_context *ctx) {
    struct inode *inode = file_inode(file);
    struct super_block *sb = inode->i_sb;
    struct buffer_head *bh;
    struct customfs_dir_entry *de;
    int err = 0;
    
    // Read directory block
    bh = sb_bread(sb, CUSTOMFS_I(inode)->direct_blocks[0]);
    if (!bh)
        return -EIO;
        
    de = (struct customfs_dir_entry *)bh->b_data;
    
    while (ctx->pos < inode->i_size) {
        if (!de->inode)
            break;
            
        if (de->name_len > 0) {
            if (!dir_emit(ctx, de->name, de->name_len,
                         le32_to_cpu(de->inode), de->file_type)) {
                err = -ENOSPC;
                break;
            }
        }
        
        ctx->pos += sizeof(struct customfs_dir_entry);
        de++;
    }
    
    brelse(bh);
    return err;
}

/* Block Management Functions */
int customfs_alloc_block(struct super_block *sb) {
    struct customfs_fs_info *fsi = CUSTOMFS_SB(sb);
    int block_nr;
    
    mutex_lock(&fsi->lock);
    
    if (fsi->sb->free_blocks == 0) {
        mutex_unlock(&fsi->lock);
        return -ENOSPC;
    }
    
    // Simple first-fit allocation (can be improved)
    block_nr = find_first_zero_bit((unsigned long *)fsi->sb->data_blocks,
                                 fsi->sb->blocks_count);
    if (block_nr >= fsi->sb->blocks_count) {
        mutex_unlock(&fsi->lock);
        return -ENOSPC;
    }
    
    // Mark block as used
    set_bit(block_nr, (unsigned long *)fsi->sb->data_blocks);
    fsi->sb->free_blocks--;
    
    mutex_unlock(&fsi->lock);
    return block_nr;
}

void customfs_free_block(struct super_block *sb, int block_nr) {
    struct customfs_fs_info *fsi = CUSTOMFS_SB(sb);
    
    mutex_lock(&fsi->lock);
    
    if (block_nr < fsi->sb->blocks_count) {
        clear_bit(block_nr, (unsigned long *)fsi->sb->data_blocks);
        fsi->sb->free_blocks++;
    }
    
    mutex_unlock(&fsi->lock);
}

/* Module Init/Exit */
static struct file_system_type customfs_type = {
    .owner = THIS_MODULE,
    .name = "customfs",
    .mount = customfs_mount,
    .kill_sb = kill_block_super,
    .fs_flags = FS_REQUIRES_DEV,
};

static int __init init_customfs(void) {
    return register_filesystem(&customfs_type);
}

static void __exit exit_customfs(void) {
    unregister_filesystem(&customfs_type);
}

module_init(init_customfs);
module_exit(exit_customfs);
