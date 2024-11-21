#ifndef MINI_RTOS_MULTIQUEUE_H
#define MINI_RTOS_MULTIQUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "hashtable.h"

/* Queue Types */
typedef enum {
    QUEUE_TYPE_FIFO = 0,
    QUEUE_TYPE_PRIORITY,
    QUEUE_TYPE_DEADLINE,
    QUEUE_TYPE_ROUND_ROBIN,
    QUEUE_TYPE_WEIGHTED
} queue_type_t;

/* Queue Policies */
typedef enum {
    QUEUE_POLICY_BLOCK = 0,
    QUEUE_POLICY_OVERWRITE,
    QUEUE_POLICY_DISCARD
} queue_policy_t;

/* Queue Statistics */
typedef struct {
    uint32_t enqueued;
    uint32_t dequeued;
    uint32_t dropped;
    uint32_t timeouts;
    uint32_t peak_usage;
    uint32_t current_usage;
} queue_stats_t;

/* Queue Item Structure */
typedef struct queue_item {
    void *data;
    uint32_t size;
    uint32_t priority;
    uint64_t deadline;
    uint32_t weight;
    struct queue_item *next;
    struct queue_item *prev;
} queue_item_t;

/* Queue Structure */
typedef struct {
    queue_type_t type;
    queue_policy_t policy;
    uint32_t max_items;
    uint32_t current_items;
    uint32_t item_size;
    queue_item_t *head;
    queue_item_t *tail;
    queue_stats_t stats;
    void *mutex;
    void *not_empty;
    void *not_full;
} queue_t;

/* Multi-Queue Manager Structure */
typedef struct {
    hashtable_t *queues;
    uint32_t active_queues;
    bool initialized;
    void *mutex;
} mqueue_manager_t;

/* Queue Error Codes */
#define QUEUE_ERR_NONE          0x00
#define QUEUE_ERR_FULL          0x01
#define QUEUE_ERR_EMPTY         0x02
#define QUEUE_ERR_TIMEOUT       0x03
#define QUEUE_ERR_INVALID       0x04
#define QUEUE_ERR_EXISTS        0x05
#define QUEUE_ERR_NOT_FOUND     0x06
#define QUEUE_ERR_NO_MEMORY     0x07

/* Multi-Queue API Functions */
void mqueue_init(void);
void mqueue_shutdown(void);

/* Queue Management */
queue_t *mqueue_create(const char *name, queue_type_t type, queue_policy_t policy,
                      uint32_t max_items, uint32_t item_size);
bool mqueue_destroy(const char *name);
queue_t *mqueue_get(const char *name);

/* Queue Operations */
bool mqueue_enqueue(queue_t *queue, void *data, uint32_t size, uint32_t priority,
                   uint64_t deadline, uint32_t weight, uint32_t timeout_ms);
bool mqueue_dequeue(queue_t *queue, void *data, uint32_t *size,
                   uint32_t timeout_ms);
bool mqueue_peek(queue_t *queue, void *data, uint32_t *size);
bool mqueue_flush(queue_t *queue);

/* Queue Information */
uint32_t mqueue_get_count(queue_t *queue);
bool mqueue_is_full(queue_t *queue);
bool mqueue_is_empty(queue_t *queue);
void mqueue_get_stats(queue_t *queue, queue_stats_t *stats);

/* Queue Configuration */
bool mqueue_set_policy(queue_t *queue, queue_policy_t policy);
bool mqueue_set_type(queue_t *queue, queue_type_t type);
bool mqueue_resize(queue_t *queue, uint32_t max_items);

/* Queue Search and Filter */
queue_item_t *mqueue_find_by_priority(queue_t *queue, uint32_t priority);
queue_item_t *mqueue_find_by_deadline(queue_t *queue, uint64_t deadline);
uint32_t mqueue_remove_expired(queue_t *queue, uint64_t current_time);

#endif /* MINI_RTOS_MULTIQUEUE_H */
