#include "atomic_ops.h"
#include <stddef.h>

#ifdef _MSC_VER
#include <windows.h>
#include <intrin.h>
#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedDecrement)
#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedCompareExchange)
#else
#include <stdatomic.h>
#endif

// Initialize atomic types
void atomic_init(atomic_t *v, int32_t i) {
    v->counter = i;
}

void atomic64_init(atomic64_t *v, int64_t i) {
    v->counter = i;
}

// Basic atomic operations
int32_t atomic_read(const atomic_t *v) {
#ifdef _MSC_VER
    return InterlockedCompareExchange((volatile LONG*)&v->counter, 0, 0);
#else
    return atomic_load((atomic_int*)&v->counter);
#endif
}

void atomic_set(atomic_t *v, int32_t i) {
#ifdef _MSC_VER
    InterlockedExchange((volatile LONG*)&v->counter, i);
#else
    atomic_store((atomic_int*)&v->counter, i);
#endif
}

void atomic_add(atomic_t *v, int32_t i) {
#ifdef _MSC_VER
    InterlockedExchangeAdd((volatile LONG*)&v->counter, i);
#else
    atomic_fetch_add((atomic_int*)&v->counter, i);
#endif
}

void atomic_sub(atomic_t *v, int32_t i) {
#ifdef _MSC_VER
    InterlockedExchangeAdd((volatile LONG*)&v->counter, -i);
#else
    atomic_fetch_sub((atomic_int*)&v->counter, i);
#endif
}

void atomic_inc(atomic_t *v) {
#ifdef _MSC_VER
    _InterlockedIncrement((volatile LONG*)&v->counter);
#else
    atomic_fetch_add((atomic_int*)&v->counter, 1);
#endif
}

void atomic_dec(atomic_t *v) {
#ifdef _MSC_VER
    _InterlockedDecrement((volatile LONG*)&v->counter);
#else
    atomic_fetch_sub((atomic_int*)&v->counter, 1);
#endif
}

// Test operations
bool atomic_dec_and_test(atomic_t *v) {
#ifdef _MSC_VER
    return _InterlockedDecrement((volatile LONG*)&v->counter) == 0;
#else
    return atomic_fetch_sub((atomic_int*)&v->counter, 1) == 1;
#endif
}

bool atomic_inc_and_test(atomic_t *v) {
#ifdef _MSC_VER
    return _InterlockedIncrement((volatile LONG*)&v->counter) == 0;
#else
    return atomic_fetch_add((atomic_int*)&v->counter, 1) == -1;
#endif
}

bool atomic_add_negative(atomic_t *v, int32_t i) {
#ifdef _MSC_VER
    return (InterlockedExchangeAdd((volatile LONG*)&v->counter, i) + i) < 0;
#else
    return (atomic_fetch_add((atomic_int*)&v->counter, i) + i) < 0;
#endif
}

// Compound operations
int32_t atomic_add_return(atomic_t *v, int32_t i) {
#ifdef _MSC_VER
    return InterlockedExchangeAdd((volatile LONG*)&v->counter, i) + i;
#else
    return atomic_fetch_add((atomic_int*)&v->counter, i) + i;
#endif
}

int32_t atomic_sub_return(atomic_t *v, int32_t i) {
#ifdef _MSC_VER
    return InterlockedExchangeAdd((volatile LONG*)&v->counter, -i) - i;
#else
    return atomic_fetch_sub((atomic_int*)&v->counter, i) - i;
#endif
}

int32_t atomic_inc_return(atomic_t *v) {
#ifdef _MSC_VER
    return _InterlockedIncrement((volatile LONG*)&v->counter);
#else
    return atomic_fetch_add((atomic_int*)&v->counter, 1) + 1;
#endif
}

int32_t atomic_dec_return(atomic_t *v) {
#ifdef _MSC_VER
    return _InterlockedDecrement((volatile LONG*)&v->counter);
#else
    return atomic_fetch_sub((atomic_int*)&v->counter, 1) - 1;
#endif
}

// Exchange operations
int32_t atomic_xchg(atomic_t *v, int32_t new) {
#ifdef _MSC_VER
    return InterlockedExchange((volatile LONG*)&v->counter, new);
#else
    return atomic_exchange((atomic_int*)&v->counter, new);
#endif
}

bool atomic_cmpxchg(atomic_t *v, int32_t old, int32_t new) {
#ifdef _MSC_VER
    return InterlockedCompareExchange((volatile LONG*)&v->counter, new, old) == old;
#else
    return atomic_compare_exchange_strong((atomic_int*)&v->counter, &old, new);
#endif
}

// 64-bit operations
int64_t atomic64_read(const atomic64_t *v) {
#ifdef _MSC_VER
    return InterlockedCompareExchange64((volatile LONG64*)&v->counter, 0, 0);
#else
    return atomic_load((atomic_long*)&v->counter);
#endif
}

void atomic64_set(atomic64_t *v, int64_t i) {
#ifdef _MSC_VER
    InterlockedExchange64((volatile LONG64*)&v->counter, i);
#else
    atomic_store((atomic_long*)&v->counter, i);
#endif
}

void atomic64_add(atomic64_t *v, int64_t i) {
#ifdef _MSC_VER
    InterlockedExchangeAdd64((volatile LONG64*)&v->counter, i);
#else
    atomic_fetch_add((atomic_long*)&v->counter, i);
#endif
}

void atomic64_sub(atomic64_t *v, int64_t i) {
#ifdef _MSC_VER
    InterlockedExchangeAdd64((volatile LONG64*)&v->counter, -i);
#else
    atomic_fetch_sub((atomic_long*)&v->counter, i);
#endif
}

void atomic64_inc(atomic64_t *v) {
#ifdef _MSC_VER
    InterlockedIncrement64((volatile LONG64*)&v->counter);
#else
    atomic_fetch_add((atomic_long*)&v->counter, 1);
#endif
}

void atomic64_dec(atomic64_t *v) {
#ifdef _MSC_VER
    InterlockedDecrement64((volatile LONG64*)&v->counter);
#else
    atomic_fetch_sub((atomic_long*)&v->counter, 1);
#endif
}

// Memory barriers
void memory_barrier(void) {
#ifdef _MSC_VER
    MemoryBarrier();
#else
    atomic_thread_fence(memory_order_seq_cst);
#endif
}

void read_memory_barrier(void) {
#ifdef _MSC_VER
    _ReadBarrier();
#else
    atomic_thread_fence(memory_order_acquire);
#endif
}

void write_memory_barrier(void) {
#ifdef _MSC_VER
    _WriteBarrier();
#else
    atomic_thread_fence(memory_order_release);
#endif
}
