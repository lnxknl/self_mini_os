#include "atomic_ops.h"
#include <stdio.h>
#include <threads.h>

// Test structure
struct test_data {
    atomic_t counter;
    atomic64_t counter64;
    int num_iterations;
};

// Thread function for increment test
int increment_thread(void* arg) {
    struct test_data* data = (struct test_data*)arg;
    
    for (int i = 0; i < data->num_iterations; i++) {
        atomic_inc(&data->counter);
        atomic64_inc(&data->counter64);
    }
    
    return 0;
}

// Thread function for compound operations test
int compound_thread(void* arg) {
    struct test_data* data = (struct test_data*)arg;
    
    for (int i = 0; i < data->num_iterations; i++) {
        atomic_add_return(&data->counter, 2);
        if (atomic_dec_and_test(&data->counter)) {
            printf("Counter reached zero!\n");
        }
    }
    
    return 0;
}

int main() {
    struct test_data data;
    const int NUM_THREADS = 4;
    const int ITERATIONS_PER_THREAD = 1000000;
    
    // Initialize atomic variables
    atomic_init(&data.counter, 0);
    atomic64_init(&data.counter64, 0);
    data.num_iterations = ITERATIONS_PER_THREAD;
    
    // Create threads for increment test
    thrd_t threads[NUM_THREADS];
    printf("Starting increment test with %d threads...\n", NUM_THREADS);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_create(&threads[i], increment_thread, &data);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_join(threads[i], NULL);
    }
    
    // Verify results
    int32_t final_count = atomic_read(&data.counter);
    int64_t final_count64 = atomic64_read(&data.counter64);
    int32_t expected_count = NUM_THREADS * ITERATIONS_PER_THREAD;
    
    printf("32-bit counter final value: %d (Expected: %d)\n", 
           final_count, expected_count);
    printf("64-bit counter final value: %lld (Expected: %d)\n", 
           (long long)final_count64, expected_count);
    
    // Reset counters for compound operations test
    atomic_set(&data.counter, 0);
    printf("\nStarting compound operations test...\n");
    
    // Create threads for compound operations test
    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_create(&threads[i], compound_thread, &data);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_join(threads[i], NULL);
    }
    
    final_count = atomic_read(&data.counter);
    printf("Final counter value after compound operations: %d\n", final_count);
    
    // Test exchange operations
    printf("\nTesting exchange operations...\n");
    atomic_set(&data.counter, 100);
    int32_t old_value = atomic_xchg(&data.counter, 200);
    printf("Exchange: old value = %d, new value = %d\n", 
           old_value, atomic_read(&data.counter));
    
    bool success = atomic_cmpxchg(&data.counter, 200, 300);
    printf("Compare and exchange: success = %s, new value = %d\n",
           success ? "true" : "false", atomic_read(&data.counter));
    
    return 0;
}
