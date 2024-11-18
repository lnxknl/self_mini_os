#include "memory_pool.h"
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/jiffies.h>

/* Pool Creation and Management */
struct mempool_config *mempool_create(const char *name, size_t elem_size,
                                    size_t min_nr, size_t max_nr,
                                    unsigned int flags) {
    struct mempool_config *pool;
    
    if (!name || !elem_size || min_nr > max_nr)
        return NULL;
        
    pool = kzalloc(sizeof(*pool), GFP_KERNEL);
    if (!pool)
        return NULL;
        
    pool->name = name;
    pool->elem_size = elem_size;
    pool->min_nr = min_nr;
    pool->max_nr = max_nr;
    pool->curr_nr = 0;
    pool->flags = flags;
    pool->gfp_mask = GFP_KERNEL;
    
    spin_lock_init(&pool->lock);
    INIT_LIST_HEAD(&pool->free_list);
    INIT_LIST_HEAD(&pool->used_list);
    
    atomic_set(&pool->state, POOL_STATE_ACTIVE);
    
    /* Initialize NUMA support if requested */
    if (flags & MEMPOOL_NUMA_AWARE) {
        pool->node_usage = kzalloc(sizeof(unsigned long) * MAX_NUMNODES,
                                 GFP_KERNEL);
        if (!pool->node_usage) {
            kfree(pool);
            return NULL;
        }
    }
    
    /* Pre-allocate elements if requested */
    if (flags & MEMPOOL_PREALLOC) {
        int i;
        for (i = 0; i < min_nr; i++) {
            struct mempool_elem *elem = kmalloc(sizeof(*elem), GFP_KERNEL);
            if (!elem)
                goto cleanup;
                
            elem->data = kmalloc(elem_size, GFP_KERNEL);
            if (!elem->data) {
                kfree(elem);
                goto cleanup;
            }
            
            elem->size = elem_size;
            elem->node_id = NUMA_NO_NODE;
            elem->flags = 0;
            elem->last_used = jiffies;
            
            list_add_tail(&elem->list, &pool->free_list);
            pool->curr_nr++;
        }
    }
    
    /* Initialize emergency pool if requested */
    if (flags & MEMPOOL_EMERGENCY) {
        if (mempool_init_emergency(pool) != 0)
            goto cleanup;
    }
    
    return pool;
    
cleanup:
    mempool_destroy(pool);
    return NULL;
}

void mempool_destroy(struct mempool_config *pool) {
    struct mempool_elem *elem, *tmp;
    unsigned long flags;
    
    if (!pool)
        return;
        
    spin_lock_irqsave(&pool->lock, flags);
    
    /* Free all elements in free list */
    list_for_each_entry_safe(elem, tmp, &pool->free_list, list) {
        list_del(&elem->list);
        kfree(elem->data);
        kfree(elem);
    }
    
    /* Free all elements in used list */
    list_for_each_entry_safe(elem, tmp, &pool->used_list, list) {
        list_del(&elem->list);
        kfree(elem->data);
        kfree(elem);
    }
    
    spin_unlock_irqrestore(&pool->lock, flags);
    
    kfree(pool->node_usage);
    kfree(pool);
}

/* Element Allocation and Management */
void *mempool_alloc(struct mempool_config *pool, gfp_t flags) {
    struct mempool_elem *elem = NULL;
    unsigned long irq_flags;
    void *ptr = NULL;
    
    if (!pool)
        return NULL;
        
    spin_lock_irqsave(&pool->lock, irq_flags);
    
    /* Try to get element from free list */
    if (!list_empty(&pool->free_list)) {
        elem = list_first_entry(&pool->free_list, struct mempool_elem, list);
        list_del(&elem->list);
        list_add_tail(&elem->list, &pool->used_list);
        elem->last_used = jiffies;
        ptr = elem->data;
        atomic_inc(&pool->stats.alloc_count);
        goto out;
    }
    
    /* Check if we can allocate new element */
    if (pool->curr_nr < pool->max_nr) {
        spin_unlock_irqrestore(&pool->lock, irq_flags);
        
        /* Allocate new element */
        elem = kmalloc(sizeof(*elem), flags);
        if (!elem)
            goto try_emergency;
            
        elem->data = kmalloc(pool->elem_size, flags);
        if (!elem->data) {
            kfree(elem);
            goto try_emergency;
        }
        
        elem->size = pool->elem_size;
        elem->node_id = NUMA_NO_NODE;
        elem->flags = 0;
        elem->last_used = jiffies;
        
        spin_lock_irqsave(&pool->lock, irq_flags);
        list_add_tail(&elem->list, &pool->used_list);
        pool->curr_nr++;
        atomic_inc(&pool->stats.alloc_count);
        ptr = elem->data;
        goto out;
    }
    
    spin_unlock_irqrestore(&pool->lock, irq_flags);
    
try_emergency:
    /* Try emergency pool if enabled */
    if (pool->flags & MEMPOOL_EMERGENCY) {
        ptr = mempool_alloc_emergency(pool);
        if (ptr) {
            atomic_inc(&pool->stats.emergency_allocs);
            return ptr;
        }
    }
    
    atomic_inc(&pool->stats.failed_allocs);
    return NULL;
    
out:
    spin_unlock_irqrestore(&pool->lock, irq_flags);
    return ptr;
}

void mempool_free(void *element, struct mempool_config *pool) {
    struct mempool_elem *elem, *found = NULL;
    unsigned long flags;
    
    if (!pool || !element)
        return;
        
    spin_lock_irqsave(&pool->lock, flags);
    
    /* Find the element in used list */
    list_for_each_entry(elem, &pool->used_list, list) {
        if (elem->data == element) {
            found = elem;
            break;
        }
    }
    
    if (found) {
        list_del(&found->list);
        list_add_tail(&found->list, &pool->free_list);
        atomic_inc(&pool->stats.free_count);
    }
    
    spin_unlock_irqrestore(&pool->lock, flags);
}

/* Emergency Pool Management */
int mempool_init_emergency(struct mempool_config *pool) {
    struct emergency_pool *epool;
    int i;
    
    if (!pool)
        return -EINVAL;
        
    epool = kzalloc(sizeof(*epool), GFP_KERNEL);
    if (!epool)
        return -ENOMEM;
        
    epool->config = pool;
    epool->nr_elements = pool->min_nr / 4; // 25% of min_nr for emergency
    
    epool->elements = kmalloc_array(epool->nr_elements,
                                  sizeof(void *), GFP_KERNEL);
    if (!epool->elements) {
        kfree(epool);
        return -ENOMEM;
    }
    
    /* Pre-allocate emergency elements */
    for (i = 0; i < epool->nr_elements; i++) {
        epool->elements[i] = kmalloc(pool->elem_size, GFP_KERNEL);
        if (!epool->elements[i]) {
            while (--i >= 0)
                kfree(epool->elements[i]);
            kfree(epool->elements);
            kfree(epool);
            return -ENOMEM;
        }
    }
    
    atomic_set(&epool->in_use, 0);
    spin_lock_init(&epool->lock);
    pool->pool_data = epool;
    
    return 0;
}

void *mempool_alloc_emergency(struct mempool_config *pool) {
    struct emergency_pool *epool;
    unsigned long flags;
    void *ptr = NULL;
    int idx;
    
    if (!pool || !pool->pool_data)
        return NULL;
        
    epool = pool->pool_data;
    
    spin_lock_irqsave(&epool->lock, flags);
    
    idx = atomic_read(&epool->in_use);
    if (idx < epool->nr_elements) {
        ptr = epool->elements[idx];
        atomic_inc(&epool->in_use);
    }
    
    spin_unlock_irqrestore(&epool->lock, flags);
    return ptr;
}

/* Pool Maintenance */
int mempool_compact(struct mempool_config *pool) {
    struct mempool_elem *elem, *tmp;
    unsigned long flags;
    int compacted = 0;
    
    if (!pool)
        return -EINVAL;
        
    spin_lock_irqsave(&pool->lock, flags);
    
    /* Remove excess free elements */
    if (pool->curr_nr > pool->min_nr) {
        list_for_each_entry_safe(elem, tmp, &pool->free_list, list) {
            if (pool->curr_nr <= pool->min_nr)
                break;
                
            list_del(&elem->list);
            kfree(elem->data);
            kfree(elem);
            pool->curr_nr--;
            compacted++;
        }
    }
    
    spin_unlock_irqrestore(&pool->lock, flags);
    return compacted;
}

void mempool_age_elements(struct mempool_config *pool) {
    struct mempool_elem *elem, *tmp;
    unsigned long flags;
    unsigned long age_threshold = jiffies - HZ * 60; // 60 seconds
    
    if (!pool)
        return;
        
    spin_lock_irqsave(&pool->lock, flags);
    
    /* Age out old free elements */
    list_for_each_entry_safe(elem, tmp, &pool->free_list, list) {
        if (elem->last_used < age_threshold) {
            list_del(&elem->list);
            kfree(elem->data);
            kfree(elem);
            pool->curr_nr--;
        }
    }
    
    spin_unlock_irqrestore(&pool->lock, flags);
}
