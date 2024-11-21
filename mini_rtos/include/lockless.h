#ifndef LOCKLESS_H
#define LOCKLESS_H

#include <stdint.h>
#include <stdbool.h>
#include "memory_order.h"

/* Lock-free queue node */
typedef struct lf_node {
    atomic_ptr_t next;
    void* data;
} lf_node_t;

/* Lock-free queue */
typedef struct {
    atomic_ptr_t head;
    atomic_ptr_t tail;
    atomic_uint32_t size;
} lf_queue_t;

/* Lock-free stack node */
typedef struct lf_stack_node {
    atomic_ptr_t next;
    void* data;
    atomic_uint32_t ref_count;
} lf_stack_node_t;

/* Lock-free stack */
typedef struct {
    atomic_ptr_t top;
    atomic_uint32_t size;
} lf_stack_t;

/* Lock-free ring buffer */
typedef struct {
    atomic_ptr_t* buffer;
    atomic_uint32_t head;
    atomic_uint32_t tail;
    uint32_t capacity;
    atomic_uint32_t size;
} lf_ring_t;

/* Lock-free hash table entry */
typedef struct lf_ht_entry {
    atomic_uint64_t key;
    atomic_ptr_t value;
    atomic_ptr_t next;
    atomic_bool marked;
} lf_ht_entry_t;

/* Lock-free hash table */
typedef struct {
    atomic_ptr_t* buckets;
    uint32_t num_buckets;
    atomic_uint32_t size;
    atomic_uint32_t resize_threshold;
} lf_hash_table_t;

/* Queue Operations */
lf_queue_t* lf_queue_create(void);
void lf_queue_destroy(lf_queue_t* queue);
bool lf_queue_enqueue(lf_queue_t* queue, void* data);
void* lf_queue_dequeue(lf_queue_t* queue);
uint32_t lf_queue_size(lf_queue_t* queue);

/* Stack Operations */
lf_stack_t* lf_stack_create(void);
void lf_stack_destroy(lf_stack_t* stack);
bool lf_stack_push(lf_stack_t* stack, void* data);
void* lf_stack_pop(lf_stack_t* stack);
uint32_t lf_stack_size(lf_stack_t* stack);

/* Ring Buffer Operations */
lf_ring_t* lf_ring_create(uint32_t capacity);
void lf_ring_destroy(lf_ring_t* ring);
bool lf_ring_enqueue(lf_ring_t* ring, void* data);
void* lf_ring_dequeue(lf_ring_t* ring);
bool lf_ring_full(lf_ring_t* ring);
bool lf_ring_empty(lf_ring_t* ring);
uint32_t lf_ring_size(lf_ring_t* ring);

/* Hash Table Operations */
lf_hash_table_t* lf_hash_create(uint32_t initial_buckets);
void lf_hash_destroy(lf_hash_table_t* table);
bool lf_hash_insert(lf_hash_table_t* table, uint64_t key, void* value);
void* lf_hash_get(lf_hash_table_t* table, uint64_t key);
bool lf_hash_remove(lf_hash_table_t* table, uint64_t key);
uint32_t lf_hash_size(lf_hash_table_t* table);

/* Memory Management */
void* lf_alloc(size_t size);
void lf_free(void* ptr);
void lf_gc_run(void);

/* Hazard Pointers */
typedef struct {
    atomic_ptr_t hazard_ptr;
    atomic_bool active;
} hazard_record_t;

void lf_hp_register_thread(void);
void lf_hp_unregister_thread(void);
void* lf_hp_protect(atomic_ptr_t* target, hazard_record_t* record);
void lf_hp_clear(hazard_record_t* record);
void lf_hp_retire(void* ptr);

#endif /* LOCKLESS_H */
