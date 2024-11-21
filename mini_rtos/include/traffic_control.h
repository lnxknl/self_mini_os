#ifndef TRAFFIC_CONTROL_H
#define TRAFFIC_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "scheduler.h"

/* Traffic class definitions */
typedef enum {
    TC_REALTIME = 0,    /* Highest priority, real-time traffic */
    TC_INTERACTIVE,     /* Interactive tasks, UI responses */
    TC_BULK,           /* Bulk data transfers */
    TC_BACKGROUND,     /* Background tasks */
    TC_IDLE,           /* Lowest priority, idle tasks */
    TC_MAX_CLASSES
} traffic_class_t;

/* Traffic control policies */
typedef enum {
    TC_POLICY_STRICT,       /* Strict priority scheduling */
    TC_POLICY_WRR,         /* Weighted Round Robin */
    TC_POLICY_DRR,         /* Deficit Round Robin */
    TC_POLICY_FAIR_QUEUE   /* Fair Queuing */
} tc_policy_t;

/* Bandwidth control parameters */
typedef struct {
    uint32_t min_rate;     /* Minimum guaranteed rate (bytes/sec) */
    uint32_t max_rate;     /* Maximum allowed rate (bytes/sec) */
    uint32_t burst_size;   /* Maximum burst size (bytes) */
    uint32_t latency;      /* Target latency (microseconds) */
} tc_bandwidth_t;

/* Traffic class configuration */
typedef struct {
    traffic_class_t class;
    tc_policy_t policy;
    tc_bandwidth_t bandwidth;
    uint32_t weight;       /* Scheduling weight */
    uint32_t quantum;      /* Time quantum for round-robin */
    uint32_t priority;     /* Base priority level */
} tc_class_config_t;

/* Traffic statistics */
typedef struct {
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t drops;
    uint32_t overruns;
    uint32_t latency_sum;
    uint32_t latency_samples;
} tc_stats_t;

/* Traffic shaper parameters */
typedef struct {
    uint32_t token_rate;       /* Tokens per second */
    uint32_t bucket_size;      /* Maximum burst size in tokens */
    uint32_t current_tokens;   /* Current token count */
    uint32_t last_update;      /* Last token update timestamp */
} tc_shaper_t;

/* Traffic control interface */
void tc_init(void);
void tc_shutdown(void);

/* Class management */
int tc_create_class(tc_class_config_t *config);
int tc_modify_class(traffic_class_t class, tc_class_config_t *config);
int tc_delete_class(traffic_class_t class);

/* Task classification */
int tc_assign_task_class(tcb_t *task, traffic_class_t class);
traffic_class_t tc_get_task_class(tcb_t *task);

/* Bandwidth control */
int tc_set_bandwidth(traffic_class_t class, tc_bandwidth_t *bandwidth);
int tc_get_bandwidth(traffic_class_t class, tc_bandwidth_t *bandwidth);

/* Traffic shaping */
bool tc_check_rate(traffic_class_t class, uint32_t size);
void tc_update_tokens(tc_shaper_t *shaper);
int tc_shape_traffic(traffic_class_t class, uint32_t size);

/* Statistics and monitoring */
void tc_get_stats(traffic_class_t class, tc_stats_t *stats);
void tc_reset_stats(traffic_class_t class);
float tc_get_average_latency(traffic_class_t class);
uint32_t tc_get_drop_rate(traffic_class_t class);

/* Policy management */
int tc_set_policy(traffic_class_t class, tc_policy_t policy);
tc_policy_t tc_get_policy(traffic_class_t class);

/* Queue management */
int tc_enqueue_task(tcb_t *task);
tcb_t *tc_dequeue_task(void);
void tc_flush_queue(traffic_class_t class);

/* Resource control */
void tc_set_cpu_share(traffic_class_t class, uint8_t percentage);
void tc_set_memory_limit(traffic_class_t class, uint32_t bytes);
void tc_set_io_priority(traffic_class_t class, uint8_t priority);

/* Debugging and maintenance */
void tc_dump_class_info(traffic_class_t class);
void tc_verify_configuration(void);
const char *tc_class_to_string(traffic_class_t class);

#endif /* TRAFFIC_CONTROL_H */
