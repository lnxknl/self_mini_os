#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include "custom_allocator.h"

#define BLOCK_MAGIC 0xDEADBEEF
#define ALIGN_SIZE  8

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Custom Allocator Developer");
MODULE_DESCRIPTION("Custom Memory Allocator");

/* Initialize memory pool */
int init_memory_pool(struct mem_pool *pool, size_t size, unsigned int flags) {
    int i;
    size_t class_size;

    if (!pool || size < MIN_BLOCK_SIZE)
        return ALLOC_ERROR_PARAM;

    /* Allocate pool memory */
    pool->pool_start = vmalloc(size);
    if (!pool->pool_start)
        return ALLOC_ERROR_NOMEM;

    pool->pool_end = pool->pool_start + size;
    pool->total_size = size;
    pool->used_size = 0;
    pool->flags = flags;

    /* Initialize size classes */
    class_size = MIN_BLOCK_SIZE;
    for (i = 0; i < NUM_SIZE_CLASSES; i++) {
        INIT_LIST_HEAD(&pool->classes[i].free_list);
        pool->classes[i].size = class_size;
        pool->classes[i].num_blocks = 0;
        pool->classes[i].max_blocks = size / (class_size * 4); /* 25% per class */
        spin_lock_init(&pool->classes[i].lock);
        class_size *= 2;
    }

    /* Initialize pool lock and large blocks list */
    spin_lock_init(&pool->pool_lock);
    INIT_LIST_HEAD(&pool->large_blocks);

    return ALLOC_SUCCESS;
}

/* Get appropriate size class for requested size */
struct size_class *get_size_class(struct mem_pool *pool, size_t size) {
    int i;
    size_t class_size = MIN_BLOCK_SIZE;

    for (i = 0; i < NUM_SIZE_CLASSES; i++) {
        if (size <= class_size)
            return &pool->classes[i];
        class_size *= 2;
    }

    return NULL; /* Size too large for size classes */
}

/* Allocate memory from pool */
void *mem_alloc(struct mem_pool *pool, size_t size) {
    struct size_class *class;
    struct mem_block *block;
    size_t aligned_size;
    unsigned long flags;

    if (!pool || !size)
        return NULL;

    /* Align size to boundary */
    aligned_size = align_size(size + sizeof(struct mem_block));

    /* Get appropriate size class */
    class = get_size_class(pool, aligned_size);

    if (class) {
        /* Allocate from size class */
        spin_lock_irqsave(&class->lock, flags);

        if (!list_empty(&class->free_list)) {
            /* Use existing block */
            block = list_first_entry(&class->free_list, 
                                   struct mem_block, list);
            list_del(&block->list);
            class->num_blocks--;
        } else {
            /* Create new block */
            if (pool->used_size + class->size > pool->total_size) {
                spin_unlock_irqrestore(&class->lock, flags);
                return NULL;
            }

            block = pool->pool_start + pool->used_size;
            block->size = class->size;
            pool->used_size += class->size;
        }

        spin_unlock_irqrestore(&class->lock, flags);
    } else {
        /* Large allocation */
        block = vmalloc(aligned_size);
        if (!block)
            return NULL;

        block->size = aligned_size;
        
        spin_lock_irqsave(&pool->pool_lock, flags);
        list_add(&block->list, &pool->large_blocks);
        spin_unlock_irqrestore(&pool->pool_lock, flags);
    }

    /* Initialize block */
    block->flags = 0;
    block->magic = BLOCK_MAGIC;

    return block->data;
}

/* Free memory block */
void mem_free(struct mem_pool *pool, void *ptr) {
    struct mem_block *block;
    struct size_class *class;
    unsigned long flags;

    if (!pool || !ptr)
        return;

    /* Get block header */
    block = container_of(ptr, struct mem_block, data);

    /* Validate block */
    if (block->magic != BLOCK_MAGIC) {
        printk(KERN_ERR "Invalid block magic: possible corruption\n");
        return;
    }

    /* Check if it's a large block */
    if (block->size > pool->classes[NUM_SIZE_CLASSES-1].size) {
        spin_lock_irqsave(&pool->pool_lock, flags);
        list_del(&block->list);
        spin_unlock_irqrestore(&pool->pool_lock, flags);
        vfree(block);
        return;
    }

    /* Get size class */
    class = get_size_class(pool, block->size);
    if (!class) {
        printk(KERN_ERR "Invalid block size\n");
        return;
    }

    /* Add to free list */
    spin_lock_irqsave(&class->lock, flags);
    list_add(&block->list, &class->free_list);
    class->num_blocks++;
    spin_unlock_irqrestore(&class->lock, flags);
}

/* Trim unused memory from pool */
int mem_pool_trim(struct mem_pool *pool) {
    struct size_class *class;
    struct mem_block *block, *tmp;
    int i;
    unsigned long flags;

    if (!pool)
        return ALLOC_ERROR_PARAM;

    /* Trim each size class */
    for (i = 0; i < NUM_SIZE_CLASSES; i++) {
        class = &pool->classes[i];
        
        spin_lock_irqsave(&class->lock, flags);
        
        list_for_each_entry_safe(block, tmp, &class->free_list, list) {
            if (class->num_blocks > class->max_blocks) {
                list_del(&block->list);
                class->num_blocks--;
                pool->used_size -= block->size;
            }
        }
        
        spin_unlock_irqrestore(&class->lock, flags);
    }

    return ALLOC_SUCCESS;
}

/* Get memory pool statistics */
void mem_pool_stats(struct mem_pool *pool, struct mem_stats *stats) {
    struct size_class *class;
    int i;
    unsigned long flags;

    if (!pool || !stats)
        return;

    memset(stats, 0, sizeof(*stats));

    /* Collect statistics from each size class */
    for (i = 0; i < NUM_SIZE_CLASSES; i++) {
        class = &pool->classes[i];
        
        spin_lock_irqsave(&class->lock, flags);
        stats->total_allocated += class->num_blocks * class->size;
        spin_unlock_irqrestore(&class->lock, flags);
    }

    stats->total_requested = pool->used_size;
    stats->peak_usage = max(stats->peak_usage, pool->used_size);
}

/* Utility functions */
size_t align_size(size_t size) {
    return (size + (ALIGN_SIZE - 1)) & ~(ALIGN_SIZE - 1);
}

int is_power_of_two(size_t size) {
    return (size & (size - 1)) == 0;
}

/* Debug functions */
void dump_pool_info(struct mem_pool *pool) {
    struct size_class *class;
    int i;

    if (!pool)
        return;

    printk(KERN_INFO "Memory Pool Info:\n");
    printk(KERN_INFO "Total Size: %zu\n", pool->total_size);
    printk(KERN_INFO "Used Size: %zu\n", pool->used_size);
    printk(KERN_INFO "Flags: 0x%x\n", pool->flags);

    for (i = 0; i < NUM_SIZE_CLASSES; i++) {
        class = &pool->classes[i];
        printk(KERN_INFO "Class %d (size %zu): %u blocks\n",
               i, class->size, class->num_blocks);
    }
}

/* Module initialization */
static int __init custom_allocator_init(void) {
    printk(KERN_INFO "Custom Memory Allocator loaded\n");
    return 0;
}

/* Module cleanup */
static void __exit custom_allocator_exit(void) {
    printk(KERN_INFO "Custom Memory Allocator unloaded\n");
}

module_init(custom_allocator_init);
module_exit(custom_allocator_exit);
