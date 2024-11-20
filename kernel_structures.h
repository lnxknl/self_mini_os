#ifndef KERNEL_STRUCTURES_H
#define KERNEL_STRUCTURES_H

#include <stdint.h>
#include <stddef.h>

/*
 * 1. Process Management Structures
 */

/* Process Descriptor */
struct task_struct {
    volatile long state;    /* -1 unrunnable, 0 runnable, >0 stopped */
    void *stack;           /* Task's stack pointer */
    int prio;             /* Process priority */
    struct mm_struct *mm;  /* Memory descriptor */
    struct files_struct *files;  /* Open files */
    struct list_head tasks;  /* Task list */
    pid_t pid;            /* Process ID */
    struct task_struct *parent;  /* Parent process */
};

/* Memory Descriptor */
struct mm_struct {
    struct vm_area_struct *mmap;    /* List of memory areas */
    unsigned long start_code;       /* Start of code segment */
    unsigned long end_code;         /* End of code segment */
    unsigned long start_data;       /* Start of data segment */
    unsigned long end_data;         /* End of data segment */
    unsigned long start_brk;        /* Start of heap */
    unsigned long brk;              /* Current heap end */
    unsigned long start_stack;      /* Start of stack */
};

/*
 * 2. Memory Management Structures
 */

/* Page Frame */
struct page {
    unsigned long flags;     /* Page flags */
    atomic_t _count;        /* Reference count */
    struct list_head lru;   /* LRU list */
    void *virtual;          /* Virtual address */
};

/* Slab Allocator */
struct kmem_cache {
    struct array_cache *array[NR_CPUS];  /* Per-CPU caches */
    unsigned int size;      /* Object size */
    unsigned int align;     /* Object alignment */
    unsigned long flags;    /* Cache flags */
    const char *name;       /* Cache name */
    struct list_head next;  /* Next cache in list */
};

/*
 * 3. File System Structures
 */

/* Inode */
struct inode {
    umode_t i_mode;              /* Access permissions */
    uid_t i_uid;                 /* User ID */
    gid_t i_gid;                /* Group ID */
    const struct inode_operations *i_op;  /* Inode operations */
    struct super_block *i_sb;    /* Superblock */
    unsigned long i_ino;         /* Inode number */
    atomic_t i_count;           /* Reference count */
    struct timespec i_atime;     /* Access time */
    struct timespec i_mtime;     /* Modification time */
    struct timespec i_ctime;     /* Change time */
};

/* Directory Entry */
struct dentry {
    struct inode *d_inode;           /* Associated inode */
    struct dentry *d_parent;         /* Parent directory */
    struct list_head d_subdirs;      /* Subdirectories */
    struct qstr d_name;              /* Entry name */
    struct super_block *d_sb;        /* Superblock */
    unsigned int d_flags;            /* Dentry flags */
};

/*
 * 4. Device Driver Structures
 */

/* Device */
struct device {
    struct device *parent;           /* Parent device */
    struct bus_type *bus;           /* Bus device is on */
    struct device_driver *driver;    /* Driver for device */
    void *platform_data;            /* Platform specific data */
    struct list_head node;          /* Node in device hierarchy */
};

/* Character Device */
struct cdev {
    struct module *owner;            /* Module owner */
    const struct file_operations *ops;  /* Device operations */
    dev_t dev;                      /* Device number */
    unsigned int count;             /* Number of devices */
};

/*
 * 5. Network Structures
 */

/* Socket Buffer */
struct sk_buff {
    struct sk_buff *next;           /* Next buffer in list */
    struct sock *sk;                /* Socket buffer belongs to */
    unsigned int len;               /* Length of actual data */
    unsigned char *data;            /* Data head pointer */
    unsigned char *head;            /* Head of buffer */
    unsigned char *tail;            /* Tail of buffer */
    unsigned char *end;             /* End of buffer */
};

/* Network Device */
struct net_device {
    char name[IFNAMSIZ];            /* Device name */
    unsigned long state;            /* Device state */
    struct net_device_stats stats;  /* Device statistics */
    const struct net_device_ops *netdev_ops;  /* Device operations */
    unsigned int flags;             /* Device flags */
    unsigned int mtu;               /* Maximum transfer unit */
};

/*
 * 6. Synchronization Structures
 */

/* Spinlock */
typedef struct {
    volatile unsigned int lock;     /* Lock variable */
} spinlock_t;

/* Mutex */
struct mutex {
    atomic_t count;                /* 1: unlocked, 0: locked */
    spinlock_t wait_lock;          /* Spinlock protecting wait list */
    struct list_head wait_list;    /* List of waiting tasks */
};

/* Read-Write Semaphore */
struct rw_semaphore {
    long count;                    /* -1: write lock, >= 0: read lock */
    spinlock_t wait_lock;          /* Spinlock for wait list */
    struct list_head wait_list;    /* List of waiting tasks */
};

/*
 * 7. Timer Structures
 */

/* Timer List */
struct timer_list {
    struct list_head entry;        /* Entry in timer list */
    unsigned long expires;         /* Expiration time */
    void (*function)(unsigned long);  /* Timer function */
    unsigned long data;            /* Function argument */
};

/* High Resolution Timer */
struct hrtimer {
    struct timerqueue_node node;   /* Timer queue node */
    ktime_t _softexpires;         /* Soft expiration time */
    enum hrtimer_restart (*function)(struct hrtimer *);  /* Timer function */
};

/*
 * 8. Block I/O Structures
 */

/* Block I/O Request */
struct bio {
    struct bio *bi_next;           /* Next BIO in chain */
    struct block_device *bi_bdev;  /* Block device */
    unsigned long bi_flags;        /* BIO flags */
    unsigned int bi_size;          /* Size in bytes */
    unsigned int bi_idx;           /* Current index in bi_io_vec */
    struct bio_vec *bi_io_vec;     /* Bio vector array */
};

/* I/O Request Queue */
struct request_queue {
    struct request *last_merge;    /* Last merged request */
    struct elevator_queue *elevator;  /* I/O scheduler */
    struct request_list rq;        /* Request list */
    unsigned int nr_requests;      /* Number of requests */
};

/*
 * 9. Virtual Memory Structures
 */

/* Virtual Memory Area */
struct vm_area_struct {
    struct mm_struct *vm_mm;       /* Associated mm_struct */
    unsigned long vm_start;        /* Start address */
    unsigned long vm_end;          /* End address */
    struct vm_area_struct *vm_next;  /* Next VMA in list */
    pgprot_t vm_page_prot;        /* Access permissions */
    unsigned long vm_flags;        /* Flags */
};

/* Page Table Entry */
typedef struct {
    unsigned long pte;            /* Page table entry */
} pte_t;

/*
 * 10. IPC Structures
 */

/* Message Queue */
struct msg_queue {
    struct list_head q_messages;   /* List of messages */
    struct list_head q_receivers;  /* List of receiving tasks */
    struct list_head q_senders;    /* List of sending tasks */
};

/* Semaphore Set */
struct sem_array {
    struct sem *sem_base;          /* Array of semaphores */
    int sem_nsems;                /* Number of semaphores */
    time_t sem_otime;             /* Last operation time */
};

#endif /* KERNEL_STRUCTURES_H */
