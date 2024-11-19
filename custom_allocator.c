#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include "custom_allocator.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Custom Allocator Developer");
MODULE_DESCRIPTION("Tree-based Custom Memory Allocator");

/* Initialize memory pool */
int init_memory_pool(struct mem_pool *pool, size_t size) {
    if (!pool || size < (1 << MIN_BLOCK_ORDER))
        return ALLOC_ERROR_PARAM;

    /* Align size to maximum block order */
    size = ALIGN(size, 1UL << MAX_BLOCK_ORDER);

    /* Allocate pool memory */
    pool->pool_start = vmalloc(size);
    if (!pool->pool_start)
        return ALLOC_ERROR_NOMEM;

    pool->pool_end = pool->pool_start + size;
    pool->total_size = size;
    pool->used_size = 0;

    /* Initialize free tree */
    pool->free_tree.root = RB_ROOT;
    spin_lock_init(&pool->free_tree.lock);

    /* Initialize free lists */
    for (int i = 0; i <= MAX_BLOCK_ORDER; i++) {
        INIT_LIST_HEAD(&pool->free_tree.free_lists[i]);
    }

    /* Create root block */
    pool->root_block = kzalloc(sizeof(struct mem_block), GFP_KERNEL);
    if (!pool->root_block) {
        vfree(pool->pool_start);
        return ALLOC_ERROR_NOMEM;
    }

    /* Initialize root block */
    pool->root_block->start_addr = pool->pool_start;
    pool->root_block->size = size;
    pool->root_block->order = ilog2(size);
    pool->root_block->flags = BLOCK_FLAG_FREE;
    pool->root_block->magic = BLOCK_MAGIC;
    
    /* Add root block to free tree */
    insert_free_block(&pool->free_tree, pool->root_block);

    return ALLOC_SUCCESS;
}

/* Find suitable free block of given order */
struct mem_block *find_free_block(struct free_tree *tree, unsigned int order) {
    struct mem_block *block;
    struct rb_node *node;
    unsigned int current_order = order;

    /* First try exact size match */
    if (!list_empty(&tree->free_lists[order])) {
        block = list_first_entry(&tree->free_lists[order], 
                               struct mem_block, buddy_list);
        return block;
    }

    /* Search for larger block that can be split */
    while (current_order <= MAX_BLOCK_ORDER) {
        if (!list_empty(&tree->free_lists[current_order])) {
            block = list_first_entry(&tree->free_lists[current_order],
                                   struct mem_block, buddy_list);
            return block;
        }
        current_order++;
    }

    return NULL;
}

/* Insert block into free tree */
void insert_free_block(struct free_tree *tree, struct mem_block *block) {
    struct rb_node **new = &tree->root.rb_node, *parent = NULL;
    struct mem_block *this; 

    /* Add to size-specific free list */
    list_add(&block->buddy_list, &tree->free_lists[block->order]);

    /* Insert into RB tree */
    while (*new) {
        parent = *new;
        this = rb_entry(parent, struct mem_block, node);

        if (block->start_addr < this->start_addr)
            new = &parent->rb_left;
        else
            new = &parent->rb_right;
    }

    rb_link_node(&block->node, parent, new);
    rb_insert_color(&block->node, &tree->root);
}

/* Remove block from free tree */
void remove_free_block(struct free_tree *tree, struct mem_block *block) {
    /* Remove from size-specific free list */
    list_del(&block->buddy_list);

    /* Remove from RB tree */
    rb_erase(&block->node, &tree->root);
}

/* Split block into smaller blocks */
struct mem_block *split_block(struct mem_pool *pool, struct mem_block *block, 
                            unsigned int target_order) {
    unsigned int current_order = block->order;
    struct mem_block *buddy;

    while (current_order > target_order) {
        current_order--;

        /* Create new buddy block */
        buddy = kzalloc(sizeof(struct mem_block), GFP_KERNEL);
        if (!buddy)
            return NULL;

        /* Configure buddy block */
        buddy->start_addr = block->start_addr + (1UL << current_order);
        buddy->size = 1UL << current_order;
        buddy->order = current_order;
        buddy->flags = BLOCK_FLAG_FREE | BLOCK_FLAG_BUDDY;
        buddy->magic = BLOCK_MAGIC;
        buddy->parent = block;

        /* Update original block */
        block->size = 1UL << current_order;
        block->order = current_order;
        block->flags |= BLOCK_FLAG_SPLIT;
        block->right = buddy;
        
        /* Add buddy to free tree */
        insert_free_block(&pool->free_tree, buddy);
    }

    return block;
}

/* Try to merge block with its buddy */
int merge_buddies(struct mem_pool *pool, struct mem_block *block) {
    struct mem_block *buddy, *parent;
    void *buddy_addr;

    while (block->parent) {
        parent = block->parent;
        
        /* Calculate buddy address */
        buddy_addr = block->start_addr ^ (1UL << block->order);
        buddy = rb_entry(rb_next(&block->node), struct mem_block, node);

        /* Check if buddy is free and same size */
        if (!buddy || buddy->start_addr != buddy_addr || 
            !(buddy->flags & BLOCK_FLAG_FREE) || 
            buddy->order != block->order)
            break;

        /* Remove both blocks from tree */
        remove_free_block(&pool->free_tree, block);
        remove_free_block(&pool->free_tree, buddy);

        /* Merge into parent */
        parent->flags &= ~BLOCK_FLAG_SPLIT;
        parent->flags |= BLOCK_FLAG_FREE;
        insert_free_block(&pool->free_tree, parent);

        block = parent;
    }

    return ALLOC_SUCCESS;
}

/* Allocate memory from pool */
void *mem_alloc(struct mem_pool *pool, size_t size) {
    struct mem_block *block;
    unsigned int order;
    unsigned long flags;

    if (!pool || !size)
        return NULL;

    /* Calculate required block order */
    order = get_block_order(size + sizeof(struct mem_block));

    /* Find and split suitable block */
    spin_lock_irqsave(&pool->free_tree.lock, flags);
    
    block = find_free_block(&pool->free_tree, order);
    if (!block) {
        spin_unlock_irqrestore(&pool->free_tree.lock, flags);
        return NULL;
    }

    /* Remove from free tree */
    remove_free_block(&pool->free_tree, block);

    /* Split if necessary */
    if (block->order > order) {
        block = split_block(pool, block, order);
        if (!block) {
            spin_unlock_irqrestore(&pool->free_tree.lock, flags);
            return NULL;
        }
    }

    /* Mark block as used */
    block->flags &= ~BLOCK_FLAG_FREE;
    pool->used_size += block->size;

    spin_unlock_irqrestore(&pool->free_tree.lock, flags);

    return block->start_addr;
}

/* Free memory block */
void mem_free(struct mem_pool *pool, void *ptr) {
    struct mem_block *block;
    unsigned long flags;

    if (!pool || !ptr)
        return;

    spin_lock_irqsave(&pool->free_tree.lock, flags);

    /* Find block in RB tree */
    block = rb_entry(pool->free_tree.root.rb_node, struct mem_block, node);
    while (block) {
        if (ptr < block->start_addr)
            block = rb_entry(block->node.rb_left, struct mem_block, node);
        else if (ptr > block->start_addr)
            block = rb_entry(block->node.rb_right, struct mem_block, node);
        else
            break;
    }

    if (!block || block->magic != BLOCK_MAGIC) {
        spin_unlock_irqrestore(&pool->free_tree.lock, flags);
        printk(KERN_ERR "Invalid free or corruption detected\n");
        return;
    }

    /* Mark block as free */
    block->flags |= BLOCK_FLAG_FREE;
    pool->used_size -= block->size;

    /* Add to free tree */
    insert_free_block(&pool->free_tree, block);

    /* Try to merge with buddies */
    merge_buddies(pool, block);

    spin_unlock_irqrestore(&pool->free_tree.lock, flags);
}

/* Utility functions */
unsigned int get_block_order(size_t size) {
    unsigned int order = MAX_BLOCK_ORDER;

    while (order >= MIN_BLOCK_ORDER) {
        if (size > (1UL << order))
            return order + 1;
        order--;
    }

    return MIN_BLOCK_ORDER;
}

/* Debug functions */
void dump_tree(struct mem_pool *pool) {
    struct rb_node *node;
    struct mem_block *block;
    int i;

    printk(KERN_INFO "Memory Pool Tree Dump:\n");
    
    /* Print free lists */
    for (i = 0; i <= MAX_BLOCK_ORDER; i++) {
        printk(KERN_INFO "Order %d free blocks:\n", i);
        list_for_each_entry(block, &pool->free_tree.free_lists[i], buddy_list) {
            printk(KERN_INFO "  Block: addr=%p size=%zu order=%u flags=%x\n",
                   block->start_addr, block->size, block->order, block->flags);
        }
    }

    /* Print RB tree */
    printk(KERN_INFO "RB Tree blocks:\n");
    for (node = rb_first(&pool->free_tree.root); node; node = rb_next(node)) {
        block = rb_entry(node, struct mem_block, node);
        printk(KERN_INFO "  Block: addr=%p size=%zu order=%u flags=%x\n",
               block->start_addr, block->size, block->order, block->flags);
    }
}

/* Module initialization */
static int __init custom_allocator_init(void) {
    printk(KERN_INFO "Tree-based Custom Memory Allocator loaded\n");
    return 0;
}

/* Module cleanup */
static void __exit custom_allocator_exit(void) {
    printk(KERN_INFO "Tree-based Custom Memory Allocator unloaded\n");
}

module_init(custom_allocator_init);
module_exit(custom_allocator_exit);
