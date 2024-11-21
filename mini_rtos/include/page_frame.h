#ifndef PAGE_FRAME_H
#define PAGE_FRAME_H

#include <stdint.h>
#include <stdbool.h>
#include "hashtable.h"

/* Page frame states */
typedef enum {
    PAGE_FREE = 0,
    PAGE_ALLOCATED,
    PAGE_RESERVED,
    PAGE_KERNEL,
    PAGE_USER,
    PAGE_SHARED
} page_state_t;

/* Page frame flags */
#define PAGE_FLAG_NONE      0x00
#define PAGE_FLAG_DIRTY     0x01    /* Page has been modified */
#define PAGE_FLAG_ACCESSED  0x02    /* Page has been accessed */
#define PAGE_FLAG_LOCKED    0x04    /* Page is locked in memory */
#define PAGE_FLAG_CACHED    0x08    /* Page is in cache */
#define PAGE_FLAG_DMA      0x10    /* Page is used for DMA */
#define PAGE_FLAG_GUARD    0x20    /* Guard page */

/* Page frame descriptor */
typedef struct page_frame {
    uint32_t pfn;              /* Page Frame Number */
    void *physical_addr;       /* Physical address */
    void *virtual_addr;        /* Virtual address (if mapped) */
    page_state_t state;        /* Current state */
    uint32_t flags;            /* Page flags */
    uint32_t ref_count;        /* Reference count */
    uint32_t owner_pid;        /* Owner process ID */
    struct page_frame *next;   /* Next page in free/allocated list */
    uint32_t last_access;      /* Last access timestamp */
    uint32_t access_count;     /* Number of times accessed */
} page_frame_t;

/* Page frame pool statistics */
typedef struct {
    uint32_t total_frames;
    uint32_t free_frames;
    uint32_t allocated_frames;
    uint32_t reserved_frames;
    uint32_t kernel_frames;
    uint32_t user_frames;
    uint32_t shared_frames;
    uint32_t peak_usage;
    uint32_t page_faults;
    uint32_t page_ins;
    uint32_t page_outs;
} page_pool_stats_t;

/* Initialization */
void page_frame_init(void *start_addr, size_t total_size, size_t page_size);

/* Page frame allocation */
page_frame_t *allocate_page_frame(void);
page_frame_t *allocate_contiguous_frames(uint32_t num_frames);
page_frame_t *allocate_aligned_frames(uint32_t num_frames, uint32_t alignment);

/* Page frame deallocation */
void free_page_frame(page_frame_t *frame);
void free_contiguous_frames(page_frame_t *start_frame, uint32_t num_frames);

/* Page frame management */
void lock_page_frame(page_frame_t *frame);
void unlock_page_frame(page_frame_t *frame);
void mark_page_accessed(page_frame_t *frame);
void mark_page_dirty(page_frame_t *frame);
void clear_page_accessed(page_frame_t *frame);
void clear_page_dirty(page_frame_t *frame);

/* Page frame lookup */
page_frame_t *get_frame_by_physical(void *phys_addr);
page_frame_t *get_frame_by_virtual(void *virt_addr);
page_frame_t *get_frame_by_pfn(uint32_t pfn);

/* Page frame pool management */
uint32_t get_free_frame_count(void);
uint32_t get_total_frame_count(void);
void get_page_pool_stats(page_pool_stats_t *stats);

/* Page replacement */
page_frame_t *find_replacement_frame(void);
void age_page_frames(void);

/* Memory region management */
bool reserve_memory_region(void *start_addr, size_t size);
void release_memory_region(void *start_addr, size_t size);

/* Debug and maintenance */
void dump_page_frame_info(page_frame_t *frame);
void verify_page_pool_integrity(void);

#endif /* PAGE_FRAME_H */
