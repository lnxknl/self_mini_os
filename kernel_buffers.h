#ifndef KERNEL_BUFFERS_H
#define KERNEL_BUFFERS_H

#include <stdint.h>
#include <stddef.h>

/*
 * 1. Socket Buffer (sk_buff)
 * Purpose: Network packet management
 * Used in: Network stack, protocol handlers
 */
struct sk_buff {
    /* Control and metadata */
    struct sk_buff *next;
    struct sk_buff *prev;
    struct sock *sk;
    struct net_device *dev;
    unsigned int len;
    
    /* Buffer pointers */
    unsigned char *head;    /* Start of allocated space */
    unsigned char *data;    /* Start of valid data */
    unsigned char *tail;    /* End of valid data */
    unsigned char *end;     /* End of allocated space */
    
    /* Protocol information */
    unsigned short protocol;
    unsigned char ip_summed;
    unsigned char cloned;
};

/*
 * 2. I/O Buffer Cache
 * Purpose: Caching disk blocks in memory
 * Used in: File systems, block devices
 */
struct buffer_head {
    unsigned long b_state;        /* Buffer state flags */
    struct buffer_head *b_next;   /* Hash queue list */
    struct buffer_head *b_prev;
    struct block_device *b_bdev;  /* Associated block device */
    sector_t b_blocknr;          /* Block number */
    size_t b_size;               /* Block size */
    unsigned char *b_data;        /* Pointer to data */
    struct page *b_page;         /* Associated page */
};

/*
 * 3. Ring Buffer (for ftrace)
 * Purpose: Kernel tracing and debugging
 * Used in: ftrace, perf events
 */
struct ring_buffer {
    unsigned long flags;
    int cpu;                    /* CPU this buffer belongs to */
    struct ring_buffer_page *head_page;    /* Head of the ring */
    struct ring_buffer_page *tail_page;    /* Tail of the ring */
    unsigned long entries;      /* Number of entries */
};

struct ring_buffer_page {
    unsigned long time_stamp;   /* Time stamp */
    void *data;                /* Actual data */
    unsigned int size;         /* Size of data */
    unsigned int padding;      /* Padding for alignment */
};

/*
 * 4. Circular Buffer (kfifo)
 * Purpose: General purpose FIFO buffer
 * Used in: Device drivers, IPC
 */
struct kfifo {
    unsigned char *buffer;     /* Buffer storage */
    unsigned int size;        /* Size of buffer */
    unsigned int in;          /* Write index */
    unsigned int out;         /* Read index */
    spinlock_t lock;         /* Buffer lock */
};

/*
 * 5. DMA Buffer
 * Purpose: Direct Memory Access operations
 * Used in: Device drivers, hardware interaction
 */
struct dma_buffer {
    void *cpu_addr;           /* CPU accessible address */
    dma_addr_t dma_addr;      /* Device accessible address */
    size_t size;             /* Buffer size */
    unsigned long flags;      /* DMA flags */
};

/*
 * 6. Page Cache
 * Purpose: Caching file pages
 * Used in: Virtual Memory System
 */
struct page_cache {
    struct address_space *mapping;  /* Owner of page cache */
    pgoff_t index;                 /* Page index in cache */
    unsigned long flags;           /* Page flags */
    atomic_t _count;              /* Reference count */
};

/*
 * 7. Slab Cache
 * Purpose: Object caching
 * Used in: Memory allocation
 */
struct kmem_cache {
    unsigned int size;            /* Object size */
    unsigned int align;           /* Object alignment */
    unsigned long flags;          /* Cache flags */
    const char *name;            /* Cache name */
    struct array_cache *array[NR_CPUS];  /* Per-CPU caches */
};

/*
 * 8. Write Buffer
 * Purpose: Delayed write operations
 * Used in: File systems, block devices
 */
struct write_buffer {
    struct list_head list;       /* Buffer list */
    void *data;                 /* Data to write */
    size_t size;               /* Size of data */
    sector_t sector;           /* Target sector */
    unsigned long expires;      /* Expiration time */
};

/*
 * 9. Read-Ahead Buffer
 * Purpose: Predictive file reading
 * Used in: File systems
 */
struct readahead_buffer {
    struct file *filp;          /* Associated file */
    pgoff_t start;             /* Start page index */
    unsigned long nr_pages;     /* Number of pages */
    unsigned long flags;        /* Buffer flags */
};

/*
 * 10. Zero-Copy Buffer
 * Purpose: Efficient data transfer
 * Used in: Network stack, file systems
 */
struct zero_copy_buffer {
    struct page **pages;        /* Array of pages */
    unsigned int nr_pages;      /* Number of pages */
    unsigned int offset;        /* Offset in first page */
    size_t len;                /* Total length */
};

/* Buffer Management Functions */

/* Initialize a circular buffer */
static inline void kfifo_init(struct kfifo *fifo, size_t size) {
    fifo->buffer = kmalloc(size, GFP_KERNEL);
    fifo->size = size;
    fifo->in = fifo->out = 0;
    spin_lock_init(&fifo->lock);
}

/* Add data to circular buffer */
static inline unsigned int kfifo_in(struct kfifo *fifo,
                                  const unsigned char *buf,
                                  unsigned int len) {
    unsigned int l;
    len = min(len, fifo->size - (fifo->in - fifo->out));
    
    /* First put the data starting from fifo->in to buffer end */
    l = min(len, fifo->size - (fifo->in & (fifo->size - 1)));
    memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buf, l);
    
    /* Then put the rest (if any) at the beginning of the buffer */
    memcpy(fifo->buffer, buf + l, len - l);
    fifo->in += len;
    return len;
}

/* Get data from circular buffer */
static inline unsigned int kfifo_out(struct kfifo *fifo,
                                   unsigned char *buf,
                                   unsigned int len) {
    unsigned int l;
    len = min(len, fifo->in - fifo->out);
    
    /* First get the data from fifo->out until the end of the buffer */
    l = min(len, fifo->size - (fifo->out & (fifo->size - 1)));
    memcpy(buf, fifo->buffer + (fifo->out & (fifo->size - 1)), l);
    
    /* Then get the rest (if any) from the beginning of the buffer */
    memcpy(buf + l, fifo->buffer, len - l);
    fifo->out += len;
    return len;
}

#endif /* KERNEL_BUFFERS_H */
