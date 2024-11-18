#ifndef _MEMORY_POOL_H
#define _MEMORY_POOL_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/atomic.h>

/* Memory Pool Types */
#define MEMPOOL_FIXED_SIZE    0x01    // Fixed-size elements
#define MEMPOOL_VARIABLE_SIZE 0x02    // Variable-size elements
#define MEMPOOL_NUMA_AWARE    0x04    // NUMA-aware allocation
#define MEMPOOL_PREALLOC      0x08    // Pre-allocate elements
#define MEMPOOL_EMERGENCY     0x10    // Emergency pool
#define MEMPOOL_CACHE_ALIGN   0x20    // Cache-aligned elements

/* Pool States */
#define POOL_STATE_ACTIVE     0x01
#define POOL_STATE_DEPLETED   0x02
#define POOL_STATE_EMERGENCY  0x04
#define POOL_STATE_RESIZING   0x08

/* Memory Pool Statistics */
struct mempool_stats {
    atomic_t alloc_count;         // Total allocations
    atomic_t free_count;          // Total frees
    atomic_t failed_allocs;       // Failed allocations
    atomic_t emergency_allocs;    // Emergency allocations
    atomic_t resize_count;        // Number of resizes
    unsigned long peak_usage;     // Peak memory usage
    unsigned long total_memory;   // Total memory allocated
    unsigned long wasted_memory;  // Wasted (fragmented) memory
};

/* Memory Pool Element */
struct mempool_elem {
    struct list_head list;        // List entry
    size_t size;                  // Element size
    void *data;                   // Element data
    int node_id;                  // NUMA node ID
    unsigned long flags;          // Element flags
    unsigned long last_used;      // Last used timestamp
};

/* Memory Pool Configuration */
struct mempool_config {
    const char *name;             // Pool name
    size_t elem_size;            // Size of each element
    size_t min_nr;               // Minimum number of elements
    size_t max_nr;               // Maximum number of elements
    size_t curr_nr;              // Current number of elements
    unsigned int flags;          // Pool flags
    gfp_t gfp_mask;             // Allocation flags
    
    /* Pool Management */
    spinlock_t lock;             // Pool lock
    struct list_head free_list;  // List of free elements
    struct list_head used_list;  // List of used elements
    
    /* NUMA Support */
    int preferred_node;          // Preferred NUMA node
    unsigned long *node_usage;   // Per-node usage statistics
    
    /* Memory Management */
    struct mempool_stats stats;  // Pool statistics
    void *pool_data;            // Pool-specific data
    atomic_t state;             // Pool state
    
    /* Callbacks */
    void *(*alloc)(size_t size, gfp_t flags, int node);
    void (*free)(void *element);
    int (*init_elem)(struct mempool_elem *elem);
    void (*deinit_elem)(struct mempool_elem *elem);
};

/* Emergency Pool */
struct emergency_pool {
    struct mempool_config *config;  // Pool configuration
    void **elements;                // Emergency elements
    size_t nr_elements;             // Number of elements
    atomic_t in_use;                // Number of elements in use
    spinlock_t lock;                // Emergency pool lock
};

/* Function Declarations */

/* Pool Creation and Destruction */
struct mempool_config *mempool_create(const char *name, size_t elem_size,
                                    size_t min_nr, size_t max_nr,
                                    unsigned int flags);
void mempool_destroy(struct mempool_config *pool);

/* Element Management */
void *mempool_alloc(struct mempool_config *pool, gfp_t flags);
void mempool_free(void *element, struct mempool_config *pool);
int mempool_resize(struct mempool_config *pool, size_t new_size);

/* NUMA Operations */
int mempool_set_node(struct mempool_config *pool, int node_id);
void *mempool_alloc_node(struct mempool_config *pool, gfp_t flags, int node_id);
struct mempool_elem *mempool_get_node_elem(struct mempool_config *pool, int node_id);

/* Emergency Pool Management */
int mempool_init_emergency(struct mempool_config *pool);
void *mempool_alloc_emergency(struct mempool_config *pool);
void mempool_free_emergency(void *element, struct mempool_config *pool);

/* Statistics and Monitoring */
void mempool_get_stats(struct mempool_config *pool, struct mempool_stats *stats);
int mempool_check_health(struct mempool_config *pool);
void mempool_dump_info(struct mempool_config *pool);

/* Pool Maintenance */
int mempool_compact(struct mempool_config *pool);
int mempool_reclaim(struct mempool_config *pool);
void mempool_age_elements(struct mempool_config *pool);

/* Helper Functions */
static inline bool mempool_is_full(struct mempool_config *pool) {
    return pool->curr_nr >= pool->max_nr;
}

static inline bool mempool_is_empty(struct mempool_config *pool) {
    return pool->curr_nr == 0;
}

static inline bool mempool_is_depleted(struct mempool_config *pool) {
    return atomic_read(&pool->state) & POOL_STATE_DEPLETED;
}

static inline size_t mempool_available(struct mempool_config *pool) {
    return pool->max_nr - pool->curr_nr;
}

#endif /* _MEMORY_POOL_H */
