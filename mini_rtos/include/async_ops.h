#ifndef ASYNC_OPS_H
#define ASYNC_OPS_H

#include <stdint.h>
#include <stdbool.h>
#include "memory_order.h"

/* Operation Status */
typedef enum {
    ASYNC_STATUS_PENDING,
    ASYNC_STATUS_IN_PROGRESS,
    ASYNC_STATUS_COMPLETED,
    ASYNC_STATUS_FAILED,
    ASYNC_STATUS_CANCELLED
} async_status_t;

/* Operation Types */
typedef enum {
    ASYNC_OP_TASK_MIGRATION,
    ASYNC_OP_LOAD_UPDATE,
    ASYNC_OP_POLICY_CHANGE,
    ASYNC_OP_THERMAL_ADJUST,
    ASYNC_OP_MEMORY_REBALANCE
} async_op_type_t;

/* Completion Callback */
typedef void (*async_callback_t)(void* context, async_status_t status, void* result);

/* Async Operation Structure */
typedef struct async_op {
    uint32_t op_id;
    async_op_type_t type;
    atomic_uint32_t status;
    void* params;
    uint32_t param_size;
    async_callback_t callback;
    void* callback_context;
    atomic_ptr_t next;
    uint64_t deadline;
    uint32_t priority;
    atomic_bool cancellable;
} async_op_t;

/* Async Queue Structure */
typedef struct {
    atomic_ptr_t head;
    atomic_ptr_t tail;
    atomic_uint32_t count;
    void* lock;
} async_queue_t;

/* Initialize async operations system */
bool async_init(uint32_t num_workers);

/* Submit an async operation */
uint32_t async_submit(async_op_type_t type, void* params, uint32_t param_size,
                     async_callback_t callback, void* context,
                     uint64_t deadline, uint32_t priority);

/* Cancel an async operation */
bool async_cancel(uint32_t op_id);

/* Wait for an async operation to complete */
bool async_wait(uint32_t op_id, uint32_t timeout_ms);

/* Get operation status */
async_status_t async_get_status(uint32_t op_id);

/* Process pending operations (called by worker threads) */
void async_process_ops(void);

/* Cleanup and shutdown async system */
void async_shutdown(void);

#endif /* ASYNC_OPS_H */
