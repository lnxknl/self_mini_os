#include "lockless.h"
#include "custom_allocator.h"
#include <string.h>

#define MAX_HAZARD_POINTERS 100
#define RETIRED_LIST_LIMIT 1000

/* Thread-local hazard pointer array */
static __thread hazard_record_t hazard_records[MAX_HAZARD_POINTERS];
static atomic_uint32_t hazard_count = 0;

/* Retired pointers list */
typedef struct retired_node {
    void* ptr;
    struct retired_node* next;
} retired_node_t;

static atomic_ptr_t retired_list = ATOMIC_PTR_INIT(NULL);
static atomic_uint32_t retired_count = 0;

/* Lock-free queue implementation */
lf_queue_t* lf_queue_create(void) {
    lf_queue_t* queue = lf_alloc(sizeof(lf_queue_t));
    if (!queue) return NULL;

    lf_node_t* dummy = lf_alloc(sizeof(lf_node_t));
    if (!dummy) {
        lf_free(queue);
        return NULL;
    }

    atomic_store_explicit(&dummy->next, NULL, MEMORY_ORDER_RELAXED);
    dummy->data = NULL;

    atomic_store_explicit(&queue->head, dummy, MEMORY_ORDER_RELAXED);
    atomic_store_explicit(&queue->tail, dummy, MEMORY_ORDER_RELAXED);
    atomic_store_explicit(&queue->size, 0, MEMORY_ORDER_RELAXED);

    return queue;
}

bool lf_queue_enqueue(lf_queue_t* queue, void* data) {
    lf_node_t* node = lf_alloc(sizeof(lf_node_t));
    if (!node) return false;

    node->data = data;
    atomic_store_explicit(&node->next, NULL, MEMORY_ORDER_RELAXED);

    while (1) {
        lf_node_t* tail = atomic_load_explicit(&queue->tail, MEMORY_ORDER_ACQUIRE);
        lf_node_t* next = atomic_load_explicit(&tail->next, MEMORY_ORDER_ACQUIRE);

        if (tail == atomic_load_explicit(&queue->tail, MEMORY_ORDER_ACQUIRE)) {
            if (next == NULL) {
                if (atomic_compare_exchange_weak_explicit(
                        &tail->next, &next, node,
                        MEMORY_ORDER_RELEASE,
                        MEMORY_ORDER_RELAXED)) {
                    atomic_compare_exchange_strong_explicit(
                        &queue->tail, &tail, node,
                        MEMORY_ORDER_RELEASE,
                        MEMORY_ORDER_RELAXED);
                    atomic_fetch_add_explicit(&queue->size, 1, MEMORY_ORDER_RELAXED);
                    return true;
                }
            } else {
                atomic_compare_exchange_weak_explicit(
                    &queue->tail, &tail, next,
                    MEMORY_ORDER_RELEASE,
                    MEMORY_ORDER_RELAXED);
            }
        }
    }
}

void* lf_queue_dequeue(lf_queue_t* queue) {
    while (1) {
        lf_node_t* head = atomic_load_explicit(&queue->head, MEMORY_ORDER_ACQUIRE);
        lf_node_t* tail = atomic_load_explicit(&queue->tail, MEMORY_ORDER_ACQUIRE);
        lf_node_t* next = atomic_load_explicit(&head->next, MEMORY_ORDER_ACQUIRE);

        if (head == atomic_load_explicit(&queue->head, MEMORY_ORDER_ACQUIRE)) {
            if (head == tail) {
                if (next == NULL) {
                    return NULL;
                }
                atomic_compare_exchange_weak_explicit(
                    &queue->tail, &tail, next,
                    MEMORY_ORDER_RELEASE,
                    MEMORY_ORDER_RELAXED);
            } else {
                void* data = next->data;
                if (atomic_compare_exchange_weak_explicit(
                        &queue->head, &head, next,
                        MEMORY_ORDER_RELEASE,
                        MEMORY_ORDER_RELAXED)) {
                    atomic_fetch_sub_explicit(&queue->size, 1, MEMORY_ORDER_RELAXED);
                    lf_hp_retire(head);
                    return data;
                }
            }
        }
    }
}

/* Lock-free stack implementation */
lf_stack_t* lf_stack_create(void) {
    lf_stack_t* stack = lf_alloc(sizeof(lf_stack_t));
    if (!stack) return NULL;

    atomic_store_explicit(&stack->top, NULL, MEMORY_ORDER_RELAXED);
    atomic_store_explicit(&stack->size, 0, MEMORY_ORDER_RELAXED);

    return stack;
}

bool lf_stack_push(lf_stack_t* stack, void* data) {
    lf_stack_node_t* node = lf_alloc(sizeof(lf_stack_node_t));
    if (!node) return false;

    node->data = data;
    atomic_store_explicit(&node->ref_count, 1, MEMORY_ORDER_RELAXED);

    do {
        lf_stack_node_t* top = atomic_load_explicit(&stack->top, MEMORY_ORDER_ACQUIRE);
        atomic_store_explicit(&node->next, top, MEMORY_ORDER_RELEASE);
    } while (!atomic_compare_exchange_weak_explicit(
        &stack->top, &node->next, node,
        MEMORY_ORDER_RELEASE,
        MEMORY_ORDER_RELAXED));

    atomic_fetch_add_explicit(&stack->size, 1, MEMORY_ORDER_RELAXED);
    return true;
}

void* lf_stack_pop(lf_stack_t* stack) {
    hazard_record_t* hp = &hazard_records[0];
    lf_stack_node_t* top;

    do {
        top = atomic_load_explicit(&stack->top, MEMORY_ORDER_ACQUIRE);
        if (top == NULL) return NULL;
        lf_hp_protect(&stack->top, hp);
        if (top != atomic_load_explicit(&stack->top, MEMORY_ORDER_ACQUIRE)) continue;

        lf_stack_node_t* next = atomic_load_explicit(&top->next, MEMORY_ORDER_ACQUIRE);
        if (atomic_compare_exchange_weak_explicit(
                &stack->top, &top, next,
                MEMORY_ORDER_RELEASE,
                MEMORY_ORDER_RELAXED)) {
            void* data = top->data;
            atomic_fetch_sub_explicit(&stack->size, 1, MEMORY_ORDER_RELAXED);
            
            if (atomic_fetch_sub_explicit(&top->ref_count, 1, MEMORY_ORDER_RELEASE) == 1) {
                lf_hp_retire(top);
            }

            lf_hp_clear(hp);
            return data;
        }
    } while (1);
}

/* Lock-free ring buffer implementation */
lf_ring_t* lf_ring_create(uint32_t capacity) {
    lf_ring_t* ring = lf_alloc(sizeof(lf_ring_t));
    if (!ring) return NULL;

    ring->buffer = lf_alloc(sizeof(atomic_ptr_t) * capacity);
    if (!ring->buffer) {
        lf_free(ring);
        return NULL;
    }

    ring->capacity = capacity;
    atomic_store_explicit(&ring->head, 0, MEMORY_ORDER_RELAXED);
    atomic_store_explicit(&ring->tail, 0, MEMORY_ORDER_RELAXED);
    atomic_store_explicit(&ring->size, 0, MEMORY_ORDER_RELAXED);

    for (uint32_t i = 0; i < capacity; i++) {
        atomic_store_explicit(&ring->buffer[i], NULL, MEMORY_ORDER_RELAXED);
    }

    return ring;
}

bool lf_ring_enqueue(lf_ring_t* ring, void* data) {
    uint32_t tail, next;

    do {
        tail = atomic_load_explicit(&ring->tail, MEMORY_ORDER_ACQUIRE);
        next = (tail + 1) % ring->capacity;

        if (next == atomic_load_explicit(&ring->head, MEMORY_ORDER_ACQUIRE)) {
            return false; // Ring is full
        }

        void* expected = NULL;
        if (!atomic_compare_exchange_weak_explicit(
                &ring->buffer[tail], &expected, data,
                MEMORY_ORDER_RELEASE,
                MEMORY_ORDER_RELAXED)) {
            continue;
        }
    } while (!atomic_compare_exchange_weak_explicit(
        &ring->tail, &tail, next,
        MEMORY_ORDER_RELEASE,
        MEMORY_ORDER_RELAXED));

    atomic_fetch_add_explicit(&ring->size, 1, MEMORY_ORDER_RELAXED);
    return true;
}

void* lf_ring_dequeue(lf_ring_t* ring) {
    uint32_t head;
    void* data;

    do {
        head = atomic_load_explicit(&ring->head, MEMORY_ORDER_ACQUIRE);
        if (head == atomic_load_explicit(&ring->tail, MEMORY_ORDER_ACQUIRE)) {
            return NULL; // Ring is empty
        }

        data = atomic_load_explicit(&ring->buffer[head], MEMORY_ORDER_ACQUIRE);
        if (!data) continue;

        void* expected = data;
        if (!atomic_compare_exchange_weak_explicit(
                &ring->buffer[head], &expected, NULL,
                MEMORY_ORDER_RELEASE,
                MEMORY_ORDER_RELAXED)) {
            continue;
        }
    } while (!atomic_compare_exchange_weak_explicit(
        &ring->head, &head, (head + 1) % ring->capacity,
        MEMORY_ORDER_RELEASE,
        MEMORY_ORDER_RELAXED));

    atomic_fetch_sub_explicit(&ring->size, 1, MEMORY_ORDER_RELAXED);
    return data;
}

/* Lock-free hash table implementation */
static uint64_t hash_function(uint64_t key) {
    key = (~key) + (key << 21);
    key = key ^ (key >> 24);
    key = (key + (key << 3)) + (key << 8);
    key = key ^ (key >> 14);
    key = (key + (key << 2)) + (key << 4);
    key = key ^ (key >> 28);
    key = key + (key << 31);
    return key;
}

lf_hash_table_t* lf_hash_create(uint32_t initial_buckets) {
    lf_hash_table_t* table = lf_alloc(sizeof(lf_hash_table_t));
    if (!table) return NULL;

    table->buckets = lf_alloc(sizeof(atomic_ptr_t) * initial_buckets);
    if (!table->buckets) {
        lf_free(table);
        return NULL;
    }

    table->num_buckets = initial_buckets;
    atomic_store_explicit(&table->size, 0, MEMORY_ORDER_RELAXED);
    atomic_store_explicit(&table->resize_threshold, initial_buckets * 3 / 4, MEMORY_ORDER_RELAXED);

    for (uint32_t i = 0; i < initial_buckets; i++) {
        atomic_store_explicit(&table->buckets[i], NULL, MEMORY_ORDER_RELAXED);
    }

    return table;
}

bool lf_hash_insert(lf_hash_table_t* table, uint64_t key, void* value) {
    uint64_t hash = hash_function(key);
    uint32_t bucket = hash % table->num_buckets;

    lf_ht_entry_t* new_entry = lf_alloc(sizeof(lf_ht_entry_t));
    if (!new_entry) return false;

    atomic_store_explicit(&new_entry->key, key, MEMORY_ORDER_RELAXED);
    atomic_store_explicit(&new_entry->value, value, MEMORY_ORDER_RELAXED);
    atomic_store_explicit(&new_entry->marked, false, MEMORY_ORDER_RELAXED);

    while (1) {
        lf_ht_entry_t* head = atomic_load_explicit(&table->buckets[bucket], MEMORY_ORDER_ACQUIRE);
        lf_ht_entry_t* current = head;

        // Search for existing key
        while (current) {
            if (atomic_load_explicit(&current->key, MEMORY_ORDER_ACQUIRE) == key &&
                !atomic_load_explicit(&current->marked, MEMORY_ORDER_ACQUIRE)) {
                // Update value if key exists
                atomic_store_explicit(&current->value, value, MEMORY_ORDER_RELEASE);
                lf_free(new_entry);
                return true;
            }
            current = atomic_load_explicit(&current->next, MEMORY_ORDER_ACQUIRE);
        }

        // Insert new entry at head
        atomic_store_explicit(&new_entry->next, head, MEMORY_ORDER_RELEASE);
        if (atomic_compare_exchange_weak_explicit(
                &table->buckets[bucket], &head, new_entry,
                MEMORY_ORDER_RELEASE,
                MEMORY_ORDER_RELAXED)) {
            atomic_fetch_add_explicit(&table->size, 1, MEMORY_ORDER_RELAXED);

            // Check if resize is needed
            if (atomic_load_explicit(&table->size, MEMORY_ORDER_RELAXED) >
                atomic_load_explicit(&table->resize_threshold, MEMORY_ORDER_RELAXED)) {
                // Trigger resize in background
                // Note: Actual resize implementation would go here
            }
            return true;
        }
    }
}

void* lf_hash_get(lf_hash_table_t* table, uint64_t key) {
    uint64_t hash = hash_function(key);
    uint32_t bucket = hash % table->num_buckets;

    hazard_record_t* hp = &hazard_records[0];
    lf_ht_entry_t* current;

    do {
        current = atomic_load_explicit(&table->buckets[bucket], MEMORY_ORDER_ACQUIRE);
        lf_hp_protect(&table->buckets[bucket], hp);
    } while (current != atomic_load_explicit(&table->buckets[bucket], MEMORY_ORDER_ACQUIRE));

    while (current) {
        if (atomic_load_explicit(&current->key, MEMORY_ORDER_ACQUIRE) == key &&
            !atomic_load_explicit(&current->marked, MEMORY_ORDER_ACQUIRE)) {
            void* value = atomic_load_explicit(&current->value, MEMORY_ORDER_ACQUIRE);
            lf_hp_clear(hp);
            return value;
        }
        current = atomic_load_explicit(&current->next, MEMORY_ORDER_ACQUIRE);
    }

    lf_hp_clear(hp);
    return NULL;
}

bool lf_hash_remove(lf_hash_table_t* table, uint64_t key) {
    uint64_t hash = hash_function(key);
    uint32_t bucket = hash % table->num_buckets;

    hazard_record_t* hp = &hazard_records[0];
    lf_ht_entry_t* current;
    lf_ht_entry_t* prev = NULL;

    do {
        current = atomic_load_explicit(&table->buckets[bucket], MEMORY_ORDER_ACQUIRE);
        lf_hp_protect(&table->buckets[bucket], hp);
    } while (current != atomic_load_explicit(&table->buckets[bucket], MEMORY_ORDER_ACQUIRE));

    while (current) {
        if (atomic_load_explicit(&current->key, MEMORY_ORDER_ACQUIRE) == key) {
            if (atomic_load_explicit(&current->marked, MEMORY_ORDER_ACQUIRE)) {
                lf_hp_clear(hp);
                return false;
            }

            atomic_store_explicit(&current->marked, true, MEMORY_ORDER_RELEASE);
            
            if (prev == NULL) {
                if (atomic_compare_exchange_weak_explicit(
                        &table->buckets[bucket], &current,
                        atomic_load_explicit(&current->next, MEMORY_ORDER_ACQUIRE),
                        MEMORY_ORDER_RELEASE,
                        MEMORY_ORDER_RELAXED)) {
                    atomic_fetch_sub_explicit(&table->size, 1, MEMORY_ORDER_RELAXED);
                    lf_hp_retire(current);
                    lf_hp_clear(hp);
                    return true;
                }
            } else {
                atomic_store_explicit(&prev->next,
                    atomic_load_explicit(&current->next, MEMORY_ORDER_ACQUIRE),
                    MEMORY_ORDER_RELEASE);
                atomic_fetch_sub_explicit(&table->size, 1, MEMORY_ORDER_RELAXED);
                lf_hp_retire(current);
                lf_hp_clear(hp);
                return true;
            }
        }
        prev = current;
        current = atomic_load_explicit(&current->next, MEMORY_ORDER_ACQUIRE);
    }

    lf_hp_clear(hp);
    return false;
}

/* Hazard pointer management */
void lf_hp_register_thread(void) {
    memset(hazard_records, 0, sizeof(hazard_records));
    for (int i = 0; i < MAX_HAZARD_POINTERS; i++) {
        atomic_store_explicit(&hazard_records[i].active, false, MEMORY_ORDER_RELAXED);
        atomic_store_explicit(&hazard_records[i].hazard_ptr, NULL, MEMORY_ORDER_RELAXED);
    }
}

void* lf_hp_protect(atomic_ptr_t* target, hazard_record_t* record) {
    void* ptr;
    do {
        ptr = atomic_load_explicit(target, MEMORY_ORDER_ACQUIRE);
        atomic_store_explicit(&record->hazard_ptr, ptr, MEMORY_ORDER_RELEASE);
        atomic_store_explicit(&record->active, true, MEMORY_ORDER_RELEASE);
    } while (ptr != atomic_load_explicit(target, MEMORY_ORDER_ACQUIRE));
    return ptr;
}

void lf_hp_clear(hazard_record_t* record) {
    atomic_store_explicit(&record->active, false, MEMORY_ORDER_RELEASE);
    atomic_store_explicit(&record->hazard_ptr, NULL, MEMORY_ORDER_RELEASE);
}

void lf_hp_retire(void* ptr) {
    retired_node_t* node = lf_alloc(sizeof(retired_node_t));
    if (!node) return;

    node->ptr = ptr;
    retired_node_t* old_head;
    do {
        old_head = atomic_load_explicit(&retired_list, MEMORY_ORDER_ACQUIRE);
        node->next = old_head;
    } while (!atomic_compare_exchange_weak_explicit(
        &retired_list, &old_head, node,
        MEMORY_ORDER_RELEASE,
        MEMORY_ORDER_RELAXED));

    if (atomic_fetch_add_explicit(&retired_count, 1, MEMORY_ORDER_RELAXED) >= RETIRED_LIST_LIMIT) {
        lf_gc_run();
    }
}

void lf_gc_run(void) {
    retired_node_t* current = atomic_exchange_explicit(&retired_list, NULL, MEMORY_ORDER_ACQUIRE);
    atomic_store_explicit(&retired_count, 0, MEMORY_ORDER_RELEASE);

    while (current) {
        bool can_free = true;
        for (int i = 0; i < MAX_HAZARD_POINTERS; i++) {
            if (atomic_load_explicit(&hazard_records[i].active, MEMORY_ORDER_ACQUIRE) &&
                atomic_load_explicit(&hazard_records[i].hazard_ptr, MEMORY_ORDER_ACQUIRE) == current->ptr) {
                can_free = false;
                break;
            }
        }

        retired_node_t* next = current->next;
        if (can_free) {
            lf_free(current->ptr);
            lf_free(current);
        } else {
            retired_node_t* node = current;
            node->next = atomic_load_explicit(&retired_list, MEMORY_ORDER_ACQUIRE);
            while (!atomic_compare_exchange_weak_explicit(
                &retired_list, &node->next, node,
                MEMORY_ORDER_RELEASE,
                MEMORY_ORDER_RELAXED));
            atomic_fetch_add_explicit(&retired_count, 1, MEMORY_ORDER_RELAXED);
        }
        current = next;
    }
}
