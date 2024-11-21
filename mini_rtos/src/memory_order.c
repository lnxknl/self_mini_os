#include "memory_order.h"
#include "rtos_core.h"

/* Memory Fence Operations */
void memory_fence_acquire(void) {
    MEMORY_BARRIER_ACQUIRE();
}

void memory_fence_release(void) {
    MEMORY_BARRIER_RELEASE();
}

void memory_fence_full(void) {
    MEMORY_BARRIER_FULL();
}

/* Atomic Load Operations */
uint32_t atomic_load_explicit_u32(const atomic_uint32_t *obj, memory_order_t order) {
    uint32_t value = obj->value;
    
    switch (order) {
        case MEMORY_ORDER_ACQUIRE:
        case MEMORY_ORDER_ACQ_REL:
        case MEMORY_ORDER_SEQ_CST:
            MEMORY_BARRIER_ACQUIRE();
            break;
        default:
            break;
    }
    
    return value;
}

uint64_t atomic_load_explicit_u64(const atomic_uint64_t *obj, memory_order_t order) {
    uint64_t value = obj->value;
    
    switch (order) {
        case MEMORY_ORDER_ACQUIRE:
        case MEMORY_ORDER_ACQ_REL:
        case MEMORY_ORDER_SEQ_CST:
            MEMORY_BARRIER_ACQUIRE();
            break;
        default:
            break;
    }
    
    return value;
}

void* atomic_load_explicit_ptr(const atomic_ptr_t *obj, memory_order_t order) {
    void* value = (void*)obj->value;
    
    switch (order) {
        case MEMORY_ORDER_ACQUIRE:
        case MEMORY_ORDER_ACQ_REL:
        case MEMORY_ORDER_SEQ_CST:
            MEMORY_BARRIER_ACQUIRE();
            break;
        default:
            break;
    }
    
    return value;
}

/* Atomic Store Operations */
void atomic_store_explicit_u32(atomic_uint32_t *obj, uint32_t value, memory_order_t order) {
    switch (order) {
        case MEMORY_ORDER_RELEASE:
        case MEMORY_ORDER_ACQ_REL:
        case MEMORY_ORDER_SEQ_CST:
            MEMORY_BARRIER_RELEASE();
            break;
        default:
            break;
    }
    
    obj->value = value;
    
    if (order == MEMORY_ORDER_SEQ_CST) {
        MEMORY_BARRIER_FULL();
    }
}

void atomic_store_explicit_u64(atomic_uint64_t *obj, uint64_t value, memory_order_t order) {
    switch (order) {
        case MEMORY_ORDER_RELEASE:
        case MEMORY_ORDER_ACQ_REL:
        case MEMORY_ORDER_SEQ_CST:
            MEMORY_BARRIER_RELEASE();
            break;
        default:
            break;
    }
    
    obj->value = value;
    
    if (order == MEMORY_ORDER_SEQ_CST) {
        MEMORY_BARRIER_FULL();
    }
}

void atomic_store_explicit_ptr(atomic_ptr_t *obj, void* value, memory_order_t order) {
    switch (order) {
        case MEMORY_ORDER_RELEASE:
        case MEMORY_ORDER_ACQ_REL:
        case MEMORY_ORDER_SEQ_CST:
            MEMORY_BARRIER_RELEASE();
            break;
        default:
            break;
    }
    
    obj->value = value;
    
    if (order == MEMORY_ORDER_SEQ_CST) {
        MEMORY_BARRIER_FULL();
    }
}

/* Compare and Exchange Operations */
bool atomic_compare_exchange_strong_explicit_u32(atomic_uint32_t *obj, uint32_t *expected,
                                               uint32_t desired, memory_order_t success_order,
                                               memory_order_t failure_order) {
    bool success = false;
    uint32_t current;
    
    /* Acquire barrier if needed */
    if (success_order >= MEMORY_ORDER_ACQUIRE || failure_order >= MEMORY_ORDER_ACQUIRE) {
        MEMORY_BARRIER_ACQUIRE();
    }
    
    /* Perform compare-exchange */
    current = obj->value;
    if (current == *expected) {
        obj->value = desired;
        success = true;
    } else {
        *expected = current;
    }
    
    /* Release barrier if needed */
    if (success && (success_order >= MEMORY_ORDER_RELEASE)) {
        MEMORY_BARRIER_RELEASE();
    }
    
    /* Full barrier for sequential consistency */
    if ((success && success_order == MEMORY_ORDER_SEQ_CST) ||
        (!success && failure_order == MEMORY_ORDER_SEQ_CST)) {
        MEMORY_BARRIER_FULL();
    }
    
    return success;
}

/* Atomic Arithmetic Operations */
uint32_t atomic_fetch_add_explicit_u32(atomic_uint32_t *obj, uint32_t value, memory_order_t order) {
    uint32_t old_val, new_val;
    
    do {
        old_val = atomic_load_explicit_u32(obj, MEMORY_ORDER_RELAXED);
        new_val = old_val + value;
    } while (!atomic_compare_exchange_strong_explicit_u32(obj, &old_val, new_val,
                                                        order, MEMORY_ORDER_RELAXED));
    
    return old_val;
}

uint32_t atomic_fetch_sub_explicit_u32(atomic_uint32_t *obj, uint32_t value, memory_order_t order) {
    uint32_t old_val, new_val;
    
    do {
        old_val = atomic_load_explicit_u32(obj, MEMORY_ORDER_RELAXED);
        new_val = old_val - value;
    } while (!atomic_compare_exchange_strong_explicit_u32(obj, &old_val, new_val,
                                                        order, MEMORY_ORDER_RELAXED));
    
    return old_val;
}

/* Cache Operations */
void cache_flush_range(void *addr, size_t size) {
    /* Architecture-specific cache flush implementation */
    /* For ARM: */
    for (size_t i = 0; i < size; i += CACHE_LINE_SIZE) {
        __asm__ volatile("dc civac, %0" :: "r"((char*)addr + i));
    }
    __asm__ volatile("dsb ish");
}

void cache_invalidate_range(void *addr, size_t size) {
    /* Architecture-specific cache invalidate implementation */
    /* For ARM: */
    for (size_t i = 0; i < size; i += CACHE_LINE_SIZE) {
        __asm__ volatile("dc ivac, %0" :: "r"((char*)addr + i));
    }
    __asm__ volatile("dsb ish");
}

void cache_clean_range(void *addr, size_t size) {
    /* Architecture-specific cache clean implementation */
    /* For ARM: */
    for (size_t i = 0; i < size; i += CACHE_LINE_SIZE) {
        __asm__ volatile("dc cvac, %0" :: "r"((char*)addr + i));
    }
    __asm__ volatile("dsb ish");
}
