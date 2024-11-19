#include "spinlock.h"
#include <stdio.h>
#include <threads.h>
#include <windows.h>

// Shared data structure
struct shared_data {
    int counter;
    spinlock_t basic_lock;
    rwspinlock_t rw_lock;
    raw_spinlock_t raw_lock;
};

// Test functions for basic spinlock
int basic_spinlock_thread(void* arg) {
    struct shared_data* data = (struct shared_data*)arg;
    
    for (int i = 0; i < 1000000; i++) {
        spin_lock(&data->basic_lock);
        data->counter++;
        spin_unlock(&data->basic_lock);
    }
    
    return 0;
}

// Test functions for reader-writer spinlock
int reader_thread(void* arg) {
    struct shared_data* data = (struct shared_data*)arg;
    int local_sum = 0;
    
    for (int i = 0; i < 100000; i++) {
        rwspin_read_lock(&data->rw_lock);
        local_sum += data->counter;  // Read operation
        rwspin_read_unlock(&data->rw_lock);
        Sleep(1);  // Small delay to simulate work
    }
    
    printf("Reader thread total sum: %d\n", local_sum);
    return 0;
}

int writer_thread(void* arg) {
    struct shared_data* data = (struct shared_data*)arg;
    
    for (int i = 0; i < 10000; i++) {
        rwspin_write_lock(&data->rw_lock);
        data->counter++;  // Write operation
        rwspin_write_unlock(&data->rw_lock);
        Sleep(5);  // Small delay to simulate work
    }
    
    return 0;
}

// Test function for raw spinlock
int raw_spinlock_thread(void* arg) {
    struct shared_data* data = (struct shared_data*)arg;
    
    for (int i = 0; i < 1000000; i++) {
        raw_spin_lock(&data->raw_lock);
        data->counter++;
        raw_spin_unlock(&data->raw_lock);
    }
    
    return 0;
}

int main() {
    struct shared_data data = {0};
    const int NUM_THREADS = 4;
    thrd_t threads[NUM_THREADS];
    
    printf("Starting spinlock tests...\n\n");
    
    // Test 1: Basic Spinlock
    printf("1. Testing basic spinlock...\n");
    spin_lock_init(&data.basic_lock);
    data.counter = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_create(&threads[i], basic_spinlock_thread, &data);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_join(threads[i], NULL);
    }
    
    printf("Basic spinlock test complete. Counter = %d (Expected: %d)\n\n",
           data.counter, NUM_THREADS * 1000000);
    
    // Test 2: Reader-Writer Spinlock
    printf("2. Testing reader-writer spinlock...\n");
    rwspin_lock_init(&data.rw_lock);
    data.counter = 0;
    
    // Create reader threads
    for (int i = 0; i < NUM_THREADS - 1; i++) {
        thrd_create(&threads[i], reader_thread, &data);
    }
    // Create one writer thread
    thrd_create(&threads[NUM_THREADS - 1], writer_thread, &data);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_join(threads[i], NULL);
    }
    
    printf("Reader-writer spinlock test complete. Final counter = %d\n\n", 
           data.counter);
    
    // Test 3: Raw Spinlock
    printf("3. Testing raw spinlock...\n");
    raw_spin_lock_init(&data.raw_lock);
    data.counter = 0;
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_create(&threads[i], raw_spinlock_thread, &data);
    }
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thrd_join(threads[i], NULL);
    }
    
    printf("Raw spinlock test complete. Counter = %d (Expected: %d)\n",
           data.counter, NUM_THREADS * 1000000);
    
    return 0;
}
