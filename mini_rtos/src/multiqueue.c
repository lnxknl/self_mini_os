#include "multiqueue.h"
#include "rtos_core.h"
#include <string.h>

/* Static Multi-Queue Manager Instance */
static mqueue_manager_t mqueue_manager = {
    .queues = NULL,
    .active_queues = 0,
    .initialized = false,
    .mutex = NULL
};

/* Initialize Multi-Queue Manager */
void mqueue_init(void) {
    if (mqueue_manager.initialized) return;
    
    enter_critical();
    
    mqueue_manager.queues = ht_create(32, ht_hash_string, ht_compare_string);
    mqueue_manager.active_queues = 0;
    mqueue_manager.mutex = mutex_create();
    mqueue_manager.initialized = true;
    
    exit_critical();
}

/* Shutdown Multi-Queue Manager */
void mqueue_shutdown(void) {
    if (!mqueue_manager.initialized) return;
    
    enter_critical();
    
    /* Destroy all queues */
    /* TODO: Implement queue iterator and cleanup */
    ht_destroy(mqueue_manager.queues);
    mutex_destroy(mqueue_manager.mutex);
    
    mqueue_manager.queues = NULL;
    mqueue_manager.active_queues = 0;
    mqueue_manager.mutex = NULL;
    mqueue_manager.initialized = false;
    
    exit_critical();
}

/* Create Queue */
queue_t *mqueue_create(const char *name, queue_type_t type, queue_policy_t policy,
                      uint32_t max_items, uint32_t item_size) {
    if (!mqueue_manager.initialized || !name || max_items == 0 || item_size == 0) {
        return NULL;
    }
    
    mutex_lock(mqueue_manager.mutex);
    
    /* Check if queue already exists */
    if (ht_get(mqueue_manager.queues, (void*)name)) {
        mutex_unlock(mqueue_manager.mutex);
        return NULL;
    }
    
    /* Allocate queue structure */
    queue_t *queue = malloc(sizeof(queue_t));
    if (!queue) {
        mutex_unlock(mqueue_manager.mutex);
        return NULL;
    }
    
    /* Initialize queue */
    queue->type = type;
    queue->policy = policy;
    queue->max_items = max_items;
    queue->current_items = 0;
    queue->item_size = item_size;
    queue->head = NULL;
    queue->tail = NULL;
    
    /* Initialize synchronization objects */
    queue->mutex = mutex_create();
    queue->not_empty = semaphore_create(0);
    queue->not_full = semaphore_create(max_items);
    
    /* Initialize statistics */
    memset(&queue->stats, 0, sizeof(queue_stats_t));
    
    /* Register queue */
    if (!ht_insert(mqueue_manager.queues, (void*)strdup(name), queue)) {
        mutex_destroy(queue->mutex);
        semaphore_destroy(queue->not_empty);
        semaphore_destroy(queue->not_full);
        free(queue);
        mutex_unlock(mqueue_manager.mutex);
        return NULL;
    }
    
    mqueue_manager.active_queues++;
    mutex_unlock(mqueue_manager.mutex);
    
    return queue;
}

/* Destroy Queue */
bool mqueue_destroy(const char *name) {
    if (!mqueue_manager.initialized || !name) return false;
    
    mutex_lock(mqueue_manager.mutex);
    
    queue_t *queue = ht_get(mqueue_manager.queues, (void*)name);
    if (!queue) {
        mutex_unlock(mqueue_manager.mutex);
        return false;
    }
    
    /* Remove from hash table */
    ht_remove(mqueue_manager.queues, (void*)name);
    
    /* Free all items */
    queue_item_t *current = queue->head;
    while (current) {
        queue_item_t *next = current->next;
        free(current->data);
        free(current);
        current = next;
    }
    
    /* Destroy synchronization objects */
    mutex_destroy(queue->mutex);
    semaphore_destroy(queue->not_empty);
    semaphore_destroy(queue->not_full);
    
    free(queue);
    mqueue_manager.active_queues--;
    
    mutex_unlock(mqueue_manager.mutex);
    return true;
}

/* Get Queue */
queue_t *mqueue_get(const char *name) {
    if (!mqueue_manager.initialized || !name) return NULL;
    return ht_get(mqueue_manager.queues, (void*)name);
}

/* Enqueue Item */
bool mqueue_enqueue(queue_t *queue, void *data, uint32_t size, uint32_t priority,
                   uint64_t deadline, uint32_t weight, uint32_t timeout_ms) {
    if (!queue || !data || size > queue->item_size) return false;
    
    /* Wait for space */
    if (!semaphore_wait(queue->not_full, timeout_ms)) {
        queue->stats.timeouts++;
        return false;
    }
    
    mutex_lock(queue->mutex);
    
    /* Create new item */
    queue_item_t *item = malloc(sizeof(queue_item_t));
    if (!item) {
        mutex_unlock(queue->mutex);
        semaphore_post(queue->not_full);
        return false;
    }
    
    item->data = malloc(size);
    if (!item->data) {
        free(item);
        mutex_unlock(queue->mutex);
        semaphore_post(queue->not_full);
        return false;
    }
    
    memcpy(item->data, data, size);
    item->size = size;
    item->priority = priority;
    item->deadline = deadline;
    item->weight = weight;
    item->next = NULL;
    item->prev = NULL;
    
    /* Insert based on queue type */
    switch (queue->type) {
        case QUEUE_TYPE_PRIORITY:
            /* Insert by priority (highest first) */
            if (!queue->head || priority > queue->head->priority) {
                item->next = queue->head;
                if (queue->head) queue->head->prev = item;
                queue->head = item;
                if (!queue->tail) queue->tail = item;
            } else {
                queue_item_t *current = queue->head;
                while (current->next && current->next->priority >= priority) {
                    current = current->next;
                }
                item->next = current->next;
                item->prev = current;
                if (current->next) current->next->prev = item;
                current->next = item;
                if (!item->next) queue->tail = item;
            }
            break;
            
        case QUEUE_TYPE_DEADLINE:
            /* Insert by deadline (earliest first) */
            if (!queue->head || deadline < queue->head->deadline) {
                item->next = queue->head;
                if (queue->head) queue->head->prev = item;
                queue->head = item;
                if (!queue->tail) queue->tail = item;
            } else {
                queue_item_t *current = queue->head;
                while (current->next && current->next->deadline <= deadline) {
                    current = current->next;
                }
                item->next = current->next;
                item->prev = current;
                if (current->next) current->next->prev = item;
                current->next = item;
                if (!item->next) queue->tail = item;
            }
            break;
            
        default:
            /* FIFO, Round Robin, Weighted: append to tail */
            if (!queue->tail) {
                queue->head = queue->tail = item;
            } else {
                item->prev = queue->tail;
                queue->tail->next = item;
                queue->tail = item;
            }
            break;
    }
    
    queue->current_items++;
    queue->stats.enqueued++;
    if (queue->current_items > queue->stats.peak_usage) {
        queue->stats.peak_usage = queue->current_items;
    }
    queue->stats.current_usage = queue->current_items;
    
    mutex_unlock(queue->mutex);
    semaphore_post(queue->not_empty);
    
    return true;
}

/* Dequeue Item */
bool mqueue_dequeue(queue_t *queue, void *data, uint32_t *size,
                   uint32_t timeout_ms) {
    if (!queue || !data || !size) return false;
    
    /* Wait for item */
    if (!semaphore_wait(queue->not_empty, timeout_ms)) {
        queue->stats.timeouts++;
        return false;
    }
    
    mutex_lock(queue->mutex);
    
    if (!queue->head) {
        mutex_unlock(queue->mutex);
        return false;
    }
    
    /* Get item based on queue type */
    queue_item_t *item = NULL;
    switch (queue->type) {
        case QUEUE_TYPE_ROUND_ROBIN:
            /* Move head to tail before dequeue */
            if (queue->head != queue->tail) {
                item = queue->head;
                queue->head = queue->head->next;
                queue->head->prev = NULL;
                item->next = NULL;
                item->prev = queue->tail;
                queue->tail->next = item;
                queue->tail = item;
            }
            /* Fall through to default dequeue */
            
        default:
            item = queue->head;
            queue->head = queue->head->next;
            if (queue->head) {
                queue->head->prev = NULL;
            } else {
                queue->tail = NULL;
            }
            break;
    }
    
    /* Copy data */
    *size = item->size;
    memcpy(data, item->data, item->size);
    
    /* Cleanup */
    free(item->data);
    free(item);
    
    queue->current_items--;
    queue->stats.dequeued++;
    queue->stats.current_usage = queue->current_items;
    
    mutex_unlock(queue->mutex);
    semaphore_post(queue->not_full);
    
    return true;
}

/* Peek Queue */
bool mqueue_peek(queue_t *queue, void *data, uint32_t *size) {
    if (!queue || !data || !size) return false;
    
    mutex_lock(queue->mutex);
    
    if (!queue->head) {
        mutex_unlock(queue->mutex);
        return false;
    }
    
    *size = queue->head->size;
    memcpy(data, queue->head->data, queue->head->size);
    
    mutex_unlock(queue->mutex);
    return true;
}

/* Flush Queue */
bool mqueue_flush(queue_t *queue) {
    if (!queue) return false;
    
    mutex_lock(queue->mutex);
    
    queue_item_t *current = queue->head;
    while (current) {
        queue_item_t *next = current->next;
        free(current->data);
        free(current);
        current = next;
        queue->stats.dropped++;
    }
    
    queue->head = NULL;
    queue->tail = NULL;
    queue->current_items = 0;
    queue->stats.current_usage = 0;
    
    /* Reset semaphores */
    semaphore_reset(queue->not_empty, 0);
    semaphore_reset(queue->not_full, queue->max_items);
    
    mutex_unlock(queue->mutex);
    return true;
}

/* Queue Information Functions */
uint32_t mqueue_get_count(queue_t *queue) {
    return queue ? queue->current_items : 0;
}

bool mqueue_is_full(queue_t *queue) {
    return queue ? queue->current_items >= queue->max_items : true;
}

bool mqueue_is_empty(queue_t *queue) {
    return queue ? queue->current_items == 0 : true;
}

void mqueue_get_stats(queue_t *queue, queue_stats_t *stats) {
    if (!queue || !stats) return;
    
    mutex_lock(queue->mutex);
    memcpy(stats, &queue->stats, sizeof(queue_stats_t));
    mutex_unlock(queue->mutex);
}

/* Queue Configuration Functions */
bool mqueue_set_policy(queue_t *queue, queue_policy_t policy) {
    if (!queue) return false;
    
    mutex_lock(queue->mutex);
    queue->policy = policy;
    mutex_unlock(queue->mutex);
    
    return true;
}

bool mqueue_set_type(queue_t *queue, queue_type_t type) {
    if (!queue) return false;
    
    mutex_lock(queue->mutex);
    queue->type = type;
    /* Reorder queue based on new type */
    /* TODO: Implement queue reordering */
    mutex_unlock(queue->mutex);
    
    return true;
}

bool mqueue_resize(queue_t *queue, uint32_t max_items) {
    if (!queue || max_items < queue->current_items) return false;
    
    mutex_lock(queue->mutex);
    queue->max_items = max_items;
    semaphore_reset(queue->not_full, max_items - queue->current_items);
    mutex_unlock(queue->mutex);
    
    return true;
}

/* Queue Search Functions */
queue_item_t *mqueue_find_by_priority(queue_t *queue, uint32_t priority) {
    if (!queue) return NULL;
    
    mutex_lock(queue->mutex);
    
    queue_item_t *current = queue->head;
    while (current && current->priority != priority) {
        current = current->next;
    }
    
    mutex_unlock(queue->mutex);
    return current;
}

queue_item_t *mqueue_find_by_deadline(queue_t *queue, uint64_t deadline) {
    if (!queue) return NULL;
    
    mutex_lock(queue->mutex);
    
    queue_item_t *current = queue->head;
    while (current && current->deadline != deadline) {
        current = current->next;
    }
    
    mutex_unlock(queue->mutex);
    return current;
}

uint32_t mqueue_remove_expired(queue_t *queue, uint64_t current_time) {
    if (!queue) return 0;
    
    mutex_lock(queue->mutex);
    
    uint32_t removed = 0;
    queue_item_t *current = queue->head;
    
    while (current) {
        if (current->deadline <= current_time) {
            queue_item_t *to_remove = current;
            current = current->next;
            
            /* Update links */
            if (to_remove->prev) {
                to_remove->prev->next = to_remove->next;
            } else {
                queue->head = to_remove->next;
            }
            
            if (to_remove->next) {
                to_remove->next->prev = to_remove->prev;
            } else {
                queue->tail = to_remove->prev;
            }
            
            /* Cleanup */
            free(to_remove->data);
            free(to_remove);
            removed++;
            queue->current_items--;
            queue->stats.dropped++;
        } else {
            current = current->next;
        }
    }
    
    queue->stats.current_usage = queue->current_items;
    
    mutex_unlock(queue->mutex);
    return removed;
}
