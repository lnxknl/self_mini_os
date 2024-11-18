#ifndef _CUSTOM_FS_H
#define _CUSTOM_FS_H

#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>

/* File System Constants */
#define CUSTOMFS_MAGIC      0x20230801
#define CUSTOMFS_BLOCK_SIZE 4096
#define CUSTOMFS_NAME_LEN   255
#define CUSTOMFS_ROOT_INO   1

/* Superblock Layout */
struct customfs_super_block {
    __le32 magic;                  /* File system magic number */
    __le32 block_size;            /* Block size in bytes */
    __le32 blocks_count;          /* Total blocks count */
    __le32 free_blocks;           /* Free blocks count */
    __le32 inodes_count;          /* Total inodes count */
    __le32 free_inodes;           /* Free inodes count */
    __le32 first_data_block;      /* First data block number */
    __le32 first_inode_block;     /* First inode block number */
    __le32 inode_blocks;          /* Number of inode blocks */
    __le32 data_blocks;           /* Number of data blocks */
    __u8   uuid[16];              /* File system UUID */
    char   volume_name[16];       /* Volume name */
    __le32 state;                 /* File system state */
    __le32 errors;                /* Error handling method */
    __le32 lastcheck;             /* Time of last check */
    __le32 checkinterval;         /* Maximum time between checks */
};

/* Inode Layout */
struct customfs_inode {
    __le16 mode;                  /* File mode */
    __le16 uid;                   /* Owner ID */
    __le16 gid;                   /* Group ID */
    __le32 size;                  /* Size in bytes */
    __le32 atime;                 /* Access time */
    __le32 mtime;                 /* Modification time */
    __le32 ctime;                 /* Creation time */
    __le32 blocks;                /* Number of blocks */
    __le32 flags;                 /* File flags */
    __le32 direct_blocks[12];     /* Direct block pointers */
    __le32 indirect_block;        /* Single indirect block pointer */
    __le32 double_indirect;       /* Double indirect block pointer */
    __le32 triple_indirect;       /* Triple indirect block pointer */
    __le32 version;               /* File version */
    __le32 generation;            /* File generation */
    __le32 file_acl;             /* File ACL */
    __le32 dir_acl;              /* Directory ACL */
    __le32 reserved[5];          /* Reserved for future use */
};

/* Directory Entry Layout */
struct customfs_dir_entry {
    __le32 inode;                /* Inode number */
    __le16 rec_len;             /* Directory entry length */
    __u8   name_len;            /* Name length */
    __u8   file_type;           /* File type */
    char   name[CUSTOMFS_NAME_LEN]; /* File name */
};

/* File System Information */
struct customfs_fs_info {
    struct buffer_head *sb_bh;           /* Super block buffer head */
    struct customfs_super_block *sb;     /* Super block data */
    unsigned long inode_table_start;     /* Start of inode table */
    unsigned long data_blocks_start;     /* Start of data blocks */
    unsigned long block_size;            /* Block size */
    unsigned long inodes_per_block;      /* Number of inodes per block */
    struct mutex lock;                   /* FS lock */
};

/* Inode Information */
struct customfs_inode_info {
    __le32 direct_blocks[12];            /* Direct block numbers */
    __le32 indirect_block;               /* Indirect block number */
    __le32 double_indirect;              /* Double indirect block */
    __le32 triple_indirect;              /* Triple indirect block */
    struct inode vfs_inode;              /* VFS inode */
};

/* File Types */
enum customfs_file_type {
    CUSTOMFS_FT_UNKNOWN,
    CUSTOMFS_FT_REG_FILE,
    CUSTOMFS_FT_DIR,
    CUSTOMFS_FT_CHRDEV,
    CUSTOMFS_FT_BLKDEV,
    CUSTOMFS_FT_FIFO,
    CUSTOMFS_FT_SOCK,
    CUSTOMFS_FT_SYMLINK,
};

/* Function Declarations */

/* Superblock Operations */
extern struct super_operations customfs_super_ops;
int customfs_fill_super(struct super_block *sb, void *data, int silent);
void customfs_put_super(struct super_block *sb);
int customfs_sync_fs(struct super_block *sb, int wait);
int customfs_statfs(struct dentry *dentry, struct kstatfs *buf);

/* Inode Operations */
extern struct inode_operations customfs_inode_ops;
struct inode *customfs_new_inode(struct super_block *sb, umode_t mode);
void customfs_free_inode(struct inode *inode);
int customfs_write_inode(struct inode *inode, struct writeback_control *wbc);
int customfs_read_inode(struct inode *inode);

/* File Operations */
extern struct file_operations customfs_file_ops;
int customfs_get_block(struct inode *inode, sector_t block, 
                      struct buffer_head *bh, int create);
int customfs_readpage(struct file *file, struct page *page);
int customfs_writepage(struct page *page, struct writeback_control *wbc);
int customfs_write_begin(struct file *file, struct address_space *mapping,
                        loff_t pos, unsigned len, unsigned flags,
                        struct page **pagep, void **fsdata);
int customfs_write_end(struct file *file, struct address_space *mapping,
                      loff_t pos, unsigned len, unsigned copied,
                      struct page *page, void *fsdata);

/* Directory Operations */
extern struct file_operations customfs_dir_ops;
int customfs_readdir(struct file *file, struct dir_context *ctx);
int customfs_create(struct inode *dir, struct dentry *dentry,
                   umode_t mode, bool excl);
int customfs_link(struct dentry *old_dentry, struct inode *dir,
                 struct dentry *dentry);
int customfs_unlink(struct inode *dir, struct dentry *dentry);
int customfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
int customfs_rmdir(struct inode *dir, struct dentry *dentry);
int customfs_rename(struct inode *old_dir, struct dentry *old_dentry,
                   struct inode *new_dir, struct dentry *new_dentry);

/* Utility Functions */
static inline struct customfs_fs_info *CUSTOMFS_SB(struct super_block *sb) {
    return sb->s_fs_info;
}

static inline struct customfs_inode_info *CUSTOMFS_I(struct inode *inode) {
    return container_of(inode, struct customfs_inode_info, vfs_inode);
}

/* Block Management */
int customfs_alloc_block(struct super_block *sb);
void customfs_free_block(struct super_block *sb, int block_nr);
int customfs_alloc_inode_block(struct super_block *sb);
void customfs_free_inode_block(struct super_block *sb, int inode_nr);

#endif /* _CUSTOM_FS_H */
