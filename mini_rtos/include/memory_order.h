#ifndef MINI_RTOS_MEMORY_ORDER_H
#define MINI_RTOS_MEMORY_ORDER_H

#include <stdint.h>
#include <stdbool.h>

/* Memory Ordering Models */
typedef enum {
    MEMORY_ORDER_RELAXED = 0,    /* No ordering constraints */
    MEMORY_ORDER_CONSUME,        /* Data dependency ordering */
    MEMORY_ORDER_ACQUIRE,        /* Acquire operation */
    MEMORY_ORDER_RELEASE,        /* Release operation */
    MEMORY_ORDER_ACQ_REL,       /* Acquire + Release */
    MEMORY_ORDER_SEQ_CST        /* Sequential consistency */
} memory_order_t;

/* Memory Barrier Types */
typedef enum {
    BARRIER_NONE = 0,
    BARRIER_LOAD,               /* Load barrier */
    BARRIER_STORE,             /* Store barrier */
    BARRIER_FULL               /* Full memory barrier */
} barrier_type_t;

/* Cache Line Size (architecture specific) */
#define CACHE_LINE_SIZE 64

/* Alignment Macros */
#define ALIGNED(x) __attribute__((aligned(x)))
#define CACHE_ALIGNED ALIGNED(CACHE_LINE_SIZE)

/* Atomic Operations */
#define ATOMIC_VAR_INIT(value) (value)
#define ATOMIC_FLAG_INIT { 0 }

/* Atomic Types */
typedef struct {
    volatile uint32_t value CACHE_ALIGNED;
} atomic_uint32_t;

typedef struct {
    volatile uint64_t value CACHE_ALIGNED;
} atomic_uint64_t;

typedef struct {
    volatile void* value CACHE_ALIGNED;
} atomic_ptr_t;

typedef struct {
    volatile uint32_t flag CACHE_ALIGNED;
} atomic_flag_t;

/* Memory Fence Operations */
void memory_fence_acquire(void);
void memory_fence_release(void);
void memory_fence_full(void);

/* Atomic Load Operations */
uint32_t atomic_load_explicit_u32(const atomic_uint32_t *obj, memory_order_t order);
uint64_t atomic_load_explicit_u64(const atomic_uint64_t *obj, memory_order_t order);
void* atomic_load_explicit_ptr(const atomic_ptr_t *obj, memory_order_t order);

/* Atomic Store Operations */
void atomic_store_explicit_u32(atomic_uint32_t *obj, uint32_t value, memory_order_t order);
void atomic_store_explicit_u64(atomic_uint64_t *obj, uint64_t value, memory_order_t order);
void atomic_store_explicit_ptr(atomic_ptr_t *obj, void* value, memory_order_t order);

/* Atomic Exchange Operations */
uint32_t atomic_exchange_explicit_u32(atomic_uint32_t *obj, uint32_t value, memory_order_t order);
uint64_t atomic_exchange_explicit_u64(atomic_uint64_t *obj, uint64_t value, memory_order_t order);
void* atomic_exchange_explicit_ptr(atomic_ptr_t *obj, void* value, memory_order_t order);

/* Compare and Exchange Operations */
bool atomic_compare_exchange_strong_explicit_u32(atomic_uint32_t *obj, uint32_t *expected,
                                               uint32_t desired, memory_order_t success_order,
                                               memory_order_t failure_order);
bool atomic_compare_exchange_strong_explicit_u64(atomic_uint64_t *obj, uint64_t *expected,
                                               uint64_t desired, memory_order_t success_order,
                                               memory_order_t failure_order);
bool atomic_compare_exchange_strong_explicit_ptr(atomic_ptr_t *obj, void **expected,
                                               void *desired, memory_order_t success_order,
                                               memory_order_t failure_order);

/* Atomic Arithmetic Operations */
uint32_t atomic_fetch_add_explicit_u32(atomic_uint32_t *obj, uint32_t value, memory_order_t order);
uint32_t atomic_fetch_sub_explicit_u32(atomic_uint32_t *obj, uint32_t value, memory_order_t order);
uint32_t atomic_fetch_and_explicit_u32(atomic_uint32_t *obj, uint32_t value, memory_order_t order);
uint32_t atomic_fetch_or_explicit_u32(atomic_uint32_t *obj, uint32_t value, memory_order_t order);
uint32_t atomic_fetch_xor_explicit_u32(atomic_uint32_t *obj, uint32_t value, memory_order_t order);

/* Memory Barrier Macros */
#define MEMORY_BARRIER_ACQUIRE() __asm__ volatile("dmb ish" ::: "memory")
#define MEMORY_BARRIER_RELEASE() __asm__ volatile("dmb ishst" ::: "memory")
#define MEMORY_BARRIER_FULL()    __asm__ volatile("dmb ish" ::: "memory")

/* Cache Operations */
void cache_flush_range(void *addr, size_t size);
void cache_invalidate_range(void *addr, size_t size);
void cache_clean_range(void *addr, size_t size);

/* Memory Access Patterns */
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

/* Prefetch Hints */
#define PREFETCH_FOR_READ(addr)  __builtin_prefetch(addr, 0, 3)
#define PREFETCH_FOR_WRITE(addr) __builtin_prefetch(addr, 1, 3)

#endif /* MINI_RTOS_MEMORY_ORDER_H */
