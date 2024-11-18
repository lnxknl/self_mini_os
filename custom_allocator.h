#ifndef _CUSTOM_ALLOCATOR_H
#define _CUSTOM_ALLOCATOR_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/gfp.h>

/* Memory block sizes */
#define MIN_BLOCK_ORDER  5    /* 32 bytes = 2^5 */
#define MAX_BLOCK_ORDER  20   /* 1MB = 2^20 */
#define BLOCK_MAGIC      0xDEADBEEF

/* Memory block flags */
#define BLOCK_FLAG_FREE     0x01
#define BLOCK_FLAG_SPLIT    0x02
#define BLOCK_FLAG_BUDDY    0x04
#define BLOCK_FLAG_LEAF     0x08

/* Error codes */
#define ALLOC_SUCCESS      0
#define ALLOC_ERROR_NOMEM -1
#define ALLOC_ERROR_PARAM -2
#define ALLOC_ERROR_INIT  -3

/* Memory block node structure */
struct mem_block {
    struct rb_node node;          /* Red-black tree node */
    struct list_head buddy_list;  /* List for buddy blocks */
    void *start_addr;             /* Start address of block */
    size_t size;                  /* Size of block */
    unsigned int order;           /* Block order (power of 2) */
    unsigned int flags;           /* Block flags */
    unsigned long magic;          /* Magic number for validation */
    struct mem_block *parent;     /* Parent block */
    struct mem_block *left;       /* Left child (lower address) */
    struct mem_block *right;      /* Right child (higher address) */
};

/* Free block tree structure */
struct free_tree {
    struct rb_root root;          /* Root of red-black tree */
    struct list_head free_lists[MAX_BLOCK_ORDER + 1];  /* Free lists by order */
    spinlock_t lock;              /* Tree lock */
};

/* Memory pool structure */
struct mem_pool {
    void *pool_start;             /* Start of pool memory */
    void *pool_end;               /* End of pool memory */
    size_t total_size;            /* Total size of pool */
    size_t used_size;            /* Currently used size */
    struct free_tree free_tree;   /* Tree of free blocks */
    spinlock_t pool_lock;         /* Pool-wide lock */
    struct mem_block *root_block; /* Root of block tree */
};

/* Statistics structure */
struct mem_stats {
    unsigned long allocs;         /* Number of allocations */
    unsigned long frees;          /* Number of frees */
    unsigned long splits;         /* Number of block splits */
    unsigned long merges;         /* Number of block merges */
    unsigned long fails;          /* Number of failed allocations */
    size_t total_requested;      /* Total memory requested */
    size_t total_allocated;      /* Total memory allocated */
    size_t peak_usage;          /* Peak memory usage */
    unsigned int fragmentation;  /* Fragmentation percentage */
};

/* Main allocator functions */
int init_memory_pool(struct mem_pool *pool, size_t size);
void *mem_alloc(struct mem_pool *pool, size_t size);
void mem_free(struct mem_pool *pool, void *ptr);
int mem_pool_destroy(struct mem_pool *pool);

/* Tree management functions */
struct mem_block *find_free_block(struct free_tree *tree, unsigned int order);
void insert_free_block(struct free_tree *tree, struct mem_block *block);
void remove_free_block(struct free_tree *tree, struct mem_block *block);
struct mem_block *split_block(struct mem_pool *pool, struct mem_block *block, unsigned int target_order);
int merge_buddies(struct mem_pool *pool, struct mem_block *block);

/* Memory pool maintenance */
int mem_pool_trim(struct mem_pool *pool);
int mem_pool_expand(struct mem_pool *pool, size_t additional_size);
void mem_pool_stats(struct mem_pool *pool, struct mem_stats *stats);

/* Debug functions */
void dump_pool_info(struct mem_pool *pool);
void dump_tree(struct mem_pool *pool);
int validate_pool(struct mem_pool *pool);
void debug_block(struct mem_block *block);

/* Utility functions */
unsigned int get_block_order(size_t size);
int is_power_of_two(size_t size);
size_t align_size(size_t size);
struct mem_block *get_buddy(struct mem_block *block);
int are_buddies(struct mem_block *block1, struct mem_block *block2);

#endif /* _CUSTOM_ALLOCATOR_H */
