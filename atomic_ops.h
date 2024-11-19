#ifndef ATOMIC_OPS_H
#define ATOMIC_OPS_H

#include <stdint.h>
#include <stdbool.h>

// Basic atomic types
typedef struct {
    volatile int32_t counter;
} atomic_t;

typedef struct {
    volatile int64_t counter;
} atomic64_t;

// Initialize atomic types
void atomic_init(atomic_t *v, int32_t i);
void atomic64_init(atomic64_t *v, int64_t i);

// Basic atomic operations
int32_t atomic_read(const atomic_t *v);
void atomic_set(atomic_t *v, int32_t i);
void atomic_add(atomic_t *v, int32_t i);
void atomic_sub(atomic_t *v, int32_t i);
void atomic_inc(atomic_t *v);
void atomic_dec(atomic_t *v);

// Test operations
bool atomic_dec_and_test(atomic_t *v);  // Returns true if result is zero
bool atomic_inc_and_test(atomic_t *v);  // Returns true if result is zero
bool atomic_add_negative(atomic_t *v, int32_t i);  // Returns true if result is negative

// Compound operations
int32_t atomic_add_return(atomic_t *v, int32_t i);
int32_t atomic_sub_return(atomic_t *v, int32_t i);
int32_t atomic_inc_return(atomic_t *v);
int32_t atomic_dec_return(atomic_t *v);

// Exchange operations
int32_t atomic_xchg(atomic_t *v, int32_t new);
bool atomic_cmpxchg(atomic_t *v, int32_t old, int32_t new);

// 64-bit operations
int64_t atomic64_read(const atomic64_t *v);
void atomic64_set(atomic64_t *v, int64_t i);
void atomic64_add(atomic64_t *v, int64_t i);
void atomic64_sub(atomic64_t *v, int64_t i);
void atomic64_inc(atomic64_t *v);
void atomic64_dec(atomic64_t *v);

// Memory barriers
void memory_barrier(void);
void read_memory_barrier(void);
void write_memory_barrier(void);

#endif // ATOMIC_OPS_H
