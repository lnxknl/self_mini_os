#ifndef _CUSTOM_ALLOCATOR_H
#define _CUSTOM_ALLOCATOR_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/gfp.h>

/* Memory block sizes */
#define MIN_BLOCK_SIZE     32
#define MAX_BLOCK_SIZE     (1 << 20)  /* 1MB */
#define NUM_SIZE_CLASSES   12         /* From 32B to 1MB */

/* Memory pool flags */
#define POOL_FLAG_CACHED   0x01    /* Use cache for fast allocation */
#define POOL_FLAG_COMPACT  0x02    /* Keep memory compact */
#define POOL_FLAG_PERSIST  0x04    /* Persistent memory pool */

/* Error codes */
#define ALLOC_SUCCESS      0
#define ALLOC_ERROR_NOMEM -1
#define ALLOC_ERROR_PARAM -2
#define ALLOC_ERROR_INIT  -3

/* Memory block header */
struct mem_block {
    size_t size;                  /* Block size including header */
    unsigned int flags;           /* Block flags */
    struct list_head list;        /* List of free/used blocks */
    unsigned long magic;          /* Magic number for validation */
    unsigned char data[0];        /* Start of actual data */
};

/* Size class information */
struct size_class {
    size_t size;                  /* Block size for this class */
    struct list_head free_list;   /* List of free blocks */
    unsigned int num_blocks;      /* Number of blocks in this class */
    unsigned int max_blocks;      /* Maximum blocks allowed */
    spinlock_t lock;             /* Lock for this size class */
};

/* Memory pool structure */
struct mem_pool {
    void *pool_start;            /* Start of pool memory */
    void *pool_end;              /* End of pool memory */
    size_t total_size;           /* Total size of pool */
    size_t used_size;           /* Currently used size */
    struct size_class classes[NUM_SIZE_CLASSES];  /* Size classes */
    spinlock_t pool_lock;        /* Pool-wide lock */
    unsigned int flags;          /* Pool flags */
    struct list_head large_blocks; /* List of large blocks */
};

/* Cache structure for frequently allocated sizes */
struct mem_cache {
    size_t obj_size;            /* Size of each object */
    struct list_head partial;   /* Partially full pages */
    struct list_head full;      /* Full pages */
    unsigned int objects_per_slab; /* Number of objects per slab */
    size_t slab_size;          /* Size of each slab */
    spinlock_t cache_lock;      /* Cache lock */
};

/* Statistics structure */
struct mem_stats {
    unsigned long allocs;       /* Number of allocations */
    unsigned long frees;        /* Number of frees */
    unsigned long fails;        /* Number of failed allocations */
    size_t total_requested;    /* Total memory requested */
    size_t total_allocated;    /* Total memory allocated */
    size_t peak_usage;        /* Peak memory usage */
};

/* Main allocator functions */
int init_memory_pool(struct mem_pool *pool, size_t size, unsigned int flags);
void *mem_alloc(struct mem_pool *pool, size_t size);
void mem_free(struct mem_pool *pool, void *ptr);
int mem_pool_destroy(struct mem_pool *pool);

/* Cache operations */
struct mem_cache *create_mem_cache(size_t size, unsigned int flags);
void *cache_alloc(struct mem_cache *cache);
void cache_free(struct mem_cache *cache, void *ptr);
void destroy_mem_cache(struct mem_cache *cache);

/* Memory pool maintenance */
int mem_pool_trim(struct mem_pool *pool);
int mem_pool_expand(struct mem_pool *pool, size_t additional_size);
void mem_pool_stats(struct mem_pool *pool, struct mem_stats *stats);

/* Debug functions */
void dump_pool_info(struct mem_pool *pool);
int validate_pool(struct mem_pool *pool);
void debug_block(struct mem_block *block);

/* Utility functions */
size_t get_block_size(size_t requested_size);
int is_power_of_two(size_t size);
size_t align_size(size_t size);
struct size_class *get_size_class(struct mem_pool *pool, size_t size);

#endif /* _CUSTOM_ALLOCATOR_H */
