#include <string.h>
#include <stdlib.h>
#include "traffic_control.h"
#include "scheduler.h"
#include "timer.h"
#include "rbtree.h"

/* Traffic class queues */
typedef struct tc_queue {
    tcb_t *head;
    tcb_t *tail;
    uint32_t count;
    uint32_t total_weight;
    tc_shaper_t shaper;
    tc_stats_t stats;
    tc_class_config_t config;
} tc_queue_t;

/* Global traffic control state */
static tc_queue_t tc_queues[TC_MAX_CLASSES];
static bool tc_initialized = false;
static uint32_t tc_current_time = 0;

/* Helper functions */
static void update_shaper(tc_shaper_t *shaper) {
    uint32_t now = get_system_time();
    uint32_t elapsed = now - shaper->last_update;
    uint32_t new_tokens = (elapsed * shaper->token_rate) / 1000; /* tokens per millisecond */
    
    shaper->current_tokens = MIN(shaper->bucket_size,
                               shaper->current_tokens + new_tokens);
    shaper->last_update = now;
}

static bool consume_tokens(tc_shaper_t *shaper, uint32_t tokens) {
    update_shaper(shaper);
    
    if (shaper->current_tokens >= tokens) {
        shaper->current_tokens -= tokens;
        return true;
    }
    return false;
}

/* Traffic control initialization */
void tc_init(void) {
    if (tc_initialized) return;
    
    /* Initialize each traffic class queue */
    for (int i = 0; i < TC_MAX_CLASSES; i++) {
        memset(&tc_queues[i], 0, sizeof(tc_queue_t));
        tc_queues[i].config.class = i;
        tc_queues[i].config.policy = TC_POLICY_STRICT;
        tc_queues[i].config.priority = TC_MAX_CLASSES - i - 1;
        
        /* Set default bandwidth parameters */
        tc_queues[i].config.bandwidth.min_rate = 0;
        tc_queues[i].config.bandwidth.max_rate = UINT32_MAX;
        tc_queues[i].config.bandwidth.burst_size = 16384;
        tc_queues[i].config.bandwidth.latency = 1000;
        
        /* Initialize shaper */
        tc_queues[i].shaper.token_rate = UINT32_MAX;
        tc_queues[i].shaper.bucket_size = 16384;
        tc_queues[i].shaper.current_tokens = 16384;
        tc_queues[i].shaper.last_update = get_system_time();
    }
    
    tc_initialized = true;
}

void tc_shutdown(void) {
    if (!tc_initialized) return;
    
    /* Flush all queues */
    for (int i = 0; i < TC_MAX_CLASSES; i++) {
        tc_flush_queue(i);
    }
    
    tc_initialized = false;
}

/* Class management */
int tc_create_class(tc_class_config_t *config) {
    if (!config || config->class >= TC_MAX_CLASSES) return -1;
    
    tc_queue_t *queue = &tc_queues[config->class];
    memcpy(&queue->config, config, sizeof(tc_class_config_t));
    
    /* Initialize shaper based on bandwidth settings */
    queue->shaper.token_rate = config->bandwidth.max_rate;
    queue->shaper.bucket_size = config->bandwidth.burst_size;
    queue->shaper.current_tokens = config->bandwidth.burst_size;
    queue->shaper.last_update = get_system_time();
    
    return 0;
}

int tc_modify_class(traffic_class_t class, tc_class_config_t *config) {
    if (!config || class >= TC_MAX_CLASSES) return -1;
    
    tc_queue_t *queue = &tc_queues[class];
    memcpy(&queue->config, config, sizeof(tc_class_config_t));
    
    /* Update shaper */
    queue->shaper.token_rate = config->bandwidth.max_rate;
    queue->shaper.bucket_size = config->bandwidth.burst_size;
    
    return 0;
}

int tc_delete_class(traffic_class_t class) {
    if (class >= TC_MAX_CLASSES) return -1;
    
    tc_flush_queue(class);
    memset(&tc_queues[class], 0, sizeof(tc_queue_t));
    
    return 0;
}

/* Task classification */
int tc_assign_task_class(tcb_t *task, traffic_class_t class) {
    if (!task || class >= TC_MAX_CLASSES) return -1;
    
    /* Store the traffic class in task's flags */
    task->flags = (task->flags & ~0xFF000000) | (class << 24);
    return 0;
}

traffic_class_t tc_get_task_class(tcb_t *task) {
    if (!task) return TC_IDLE;
    return (task->flags >> 24) & 0xFF;
}

/* Queue management */
int tc_enqueue_task(tcb_t *task) {
    if (!task) return -1;
    
    traffic_class_t class = tc_get_task_class(task);
    tc_queue_t *queue = &tc_queues[class];
    
    /* Check bandwidth limits */
    if (!tc_check_rate(class, 1)) {
        queue->stats.drops++;
        return -1;
    }
    
    /* Add to queue */
    if (!queue->head) {
        queue->head = queue->tail = task;
    } else {
        queue->tail->next = task;
        queue->tail = task;
    }
    task->next = NULL;
    queue->count++;
    
    return 0;
}

tcb_t *tc_dequeue_task(void) {
    /* Start from highest priority class */
    for (int i = 0; i < TC_MAX_CLASSES; i++) {
        tc_queue_t *queue = &tc_queues[i];
        
        if (queue->head) {
            tcb_t *task = queue->head;
            queue->head = task->next;
            if (!queue->head) queue->tail = NULL;
            task->next = NULL;
            queue->count--;
            
            /* Update statistics */
            queue->stats.packets_sent++;
            
            return task;
        }
    }
    
    return NULL;
}

void tc_flush_queue(traffic_class_t class) {
    if (class >= TC_MAX_CLASSES) return;
    
    tc_queue_t *queue = &tc_queues[class];
    queue->head = queue->tail = NULL;
    queue->count = 0;
}

/* Traffic shaping */
bool tc_check_rate(traffic_class_t class, uint32_t size) {
    if (class >= TC_MAX_CLASSES) return false;
    
    tc_queue_t *queue = &tc_queues[class];
    return consume_tokens(&queue->shaper, size);
}

void tc_update_tokens(tc_shaper_t *shaper) {
    if (!shaper) return;
    update_shaper(shaper);
}

int tc_shape_traffic(traffic_class_t class, uint32_t size) {
    if (class >= TC_MAX_CLASSES) return -1;
    
    tc_queue_t *queue = &tc_queues[class];
    if (!tc_check_rate(class, size)) {
        queue->stats.drops++;
        return -1;
    }
    
    return 0;
}

/* Statistics and monitoring */
void tc_get_stats(traffic_class_t class, tc_stats_t *stats) {
    if (class >= TC_MAX_CLASSES || !stats) return;
    
    memcpy(stats, &tc_queues[class].stats, sizeof(tc_stats_t));
}

void tc_reset_stats(traffic_class_t class) {
    if (class >= TC_MAX_CLASSES) return;
    
    memset(&tc_queues[class].stats, 0, sizeof(tc_stats_t));
}

float tc_get_average_latency(traffic_class_t class) {
    if (class >= TC_MAX_CLASSES) return 0.0f;
    
    tc_stats_t *stats = &tc_queues[class].stats;
    if (stats->latency_samples == 0) return 0.0f;
    
    return (float)stats->latency_sum / stats->latency_samples;
}

uint32_t tc_get_drop_rate(traffic_class_t class) {
    if (class >= TC_MAX_CLASSES) return 0;
    
    tc_stats_t *stats = &tc_queues[class].stats;
    uint32_t total = stats->packets_sent + stats->drops;
    if (total == 0) return 0;
    
    return (stats->drops * 100) / total;
}

/* Policy management */
int tc_set_policy(traffic_class_t class, tc_policy_t policy) {
    if (class >= TC_MAX_CLASSES) return -1;
    
    tc_queues[class].config.policy = policy;
    return 0;
}

tc_policy_t tc_get_policy(traffic_class_t class) {
    if (class >= TC_MAX_CLASSES) return TC_POLICY_STRICT;
    
    return tc_queues[class].config.policy;
}

/* Resource control */
void tc_set_cpu_share(traffic_class_t class, uint8_t percentage) {
    if (class >= TC_MAX_CLASSES || percentage > 100) return;
    
    tc_queue_t *queue = &tc_queues[class];
    queue->config.weight = percentage;
}

void tc_set_memory_limit(traffic_class_t class, uint32_t bytes) {
    if (class >= TC_MAX_CLASSES) return;
    
    tc_queue_t *queue = &tc_queues[class];
    queue->config.bandwidth.max_rate = bytes;
}

void tc_set_io_priority(traffic_class_t class, uint8_t priority) {
    if (class >= TC_MAX_CLASSES) return;
    
    tc_queue_t *queue = &tc_queues[class];
    queue->config.priority = priority;
}

/* Debugging and maintenance */
void tc_dump_class_info(traffic_class_t class) {
    if (class >= TC_MAX_CLASSES) return;
    
    tc_queue_t *queue = &tc_queues[class];
    printf("Traffic Class %d (%s):\n", class, tc_class_to_string(class));
    printf("  Policy: %d\n", queue->config.policy);
    printf("  Tasks: %d\n", queue->count);
    printf("  Bandwidth: %d/%d bytes/sec\n",
           queue->config.bandwidth.min_rate,
           queue->config.bandwidth.max_rate);
    printf("  Drops: %d\n", queue->stats.drops);
    printf("  Average Latency: %.2f us\n", tc_get_average_latency(class));
}

void tc_verify_configuration(void) {
    for (int i = 0; i < TC_MAX_CLASSES; i++) {
        tc_queue_t *queue = &tc_queues[i];
        
        /* Verify queue integrity */
        uint32_t count = 0;
        tcb_t *task = queue->head;
        while (task) {
            count++;
            task = task->next;
        }
        
        if (count != queue->count) {
            printf("Warning: Class %d queue count mismatch (%d != %d)\n",
                   i, count, queue->count);
        }
    }
}

const char *tc_class_to_string(traffic_class_t class) {
    static const char *class_names[] = {
        "REALTIME",
        "INTERACTIVE",
        "BULK",
        "BACKGROUND",
        "IDLE"
    };
    
    if (class >= TC_MAX_CLASSES) return "UNKNOWN";
    return class_names[class];
}
