#include "async_ops.h"
#include "memory_order.h"
#include <string.h>
#include <stdlib.h>

#define MAX_WORKERS 16
#define MAX_PENDING_OPS 1024

/* Worker Thread State */
typedef struct {
    atomic_bool active;
    void* thread;
    async_queue_t queue;
} worker_state_t;

/* Global Async System State */
static struct {
    atomic_bool initialized;
    atomic_uint32_t next_op_id;
    worker_state_t workers[MAX_WORKERS];
    uint32_t num_workers;
    async_op_t* op_pool;
    atomic_uint32_t op_pool_index;
    void* global_lock;
} async_state;

/* Initialize worker thread */
static bool init_worker(worker_state_t* worker) {
    atomic_store_explicit_bool(&worker->active, true, MEMORY_ORDER_RELEASE);
    worker->queue.head = ATOMIC_VAR_INIT(NULL);
    worker->queue.tail = ATOMIC_VAR_INIT(NULL);
    atomic_store_explicit_u32(&worker->queue.count, 0, MEMORY_ORDER_RELEASE);
    worker->queue.lock = mutex_create();
    
    // Create worker thread
    worker->thread = thread_create(async_process_ops, NULL);
    return worker->thread != NULL;
}

/* Initialize async operations system */
bool async_init(uint32_t num_workers) {
    bool expected = false;
    if (!atomic_compare_exchange_strong_explicit_bool(
            &async_state.initialized, &expected, true,
            MEMORY_ORDER_ACQ_REL, MEMORY_ORDER_RELAXED)) {
        return false;
    }

    if (num_workers > MAX_WORKERS) {
        num_workers = MAX_WORKERS;
    }

    async_state.num_workers = num_workers;
    atomic_store_explicit_u32(&async_state.next_op_id, 1, MEMORY_ORDER_RELEASE);
    atomic_store_explicit_u32(&async_state.op_pool_index, 0, MEMORY_ORDER_RELEASE);
    
    // Initialize operation pool
    async_state.op_pool = calloc(MAX_PENDING_OPS, sizeof(async_op_t));
    if (!async_state.op_pool) {
        return false;
    }

    async_state.global_lock = mutex_create();
    if (!async_state.global_lock) {
        free(async_state.op_pool);
        return false;
    }

    // Initialize workers
    for (uint32_t i = 0; i < num_workers; i++) {
        if (!init_worker(&async_state.workers[i])) {
            async_shutdown();
            return false;
        }
    }

    return true;
}

/* Get next available operation from pool */
static async_op_t* get_op_from_pool(void) {
    uint32_t index = atomic_fetch_add_explicit_u32(
        &async_state.op_pool_index, 1, MEMORY_ORDER_ACQ_REL) % MAX_PENDING_OPS;
    return &async_state.op_pool[index];
}

/* Select worker based on operation type and current load */
static worker_state_t* select_worker(async_op_type_t type, uint32_t priority) {
    worker_state_t* best_worker = &async_state.workers[0];
    uint32_t min_count = UINT32_MAX;

    for (uint32_t i = 0; i < async_state.num_workers; i++) {
        worker_state_t* worker = &async_state.workers[i];
        uint32_t count = atomic_load_explicit_u32(&worker->queue.count, MEMORY_ORDER_ACQUIRE);
        
        if (count < min_count) {
            min_count = count;
            best_worker = worker;
        }
    }

    return best_worker;
}

/* Submit an async operation */
uint32_t async_submit(async_op_type_t type, void* params, uint32_t param_size,
                     async_callback_t callback, void* context,
                     uint64_t deadline, uint32_t priority) {
    if (!atomic_load_explicit_bool(&async_state.initialized, MEMORY_ORDER_ACQUIRE)) {
        return 0;
    }

    // Prepare operation
    async_op_t* op = get_op_from_pool();
    op->op_id = atomic_fetch_add_explicit_u32(&async_state.next_op_id, 1, MEMORY_ORDER_ACQ_REL);
    op->type = type;
    atomic_store_explicit_u32((atomic_uint32_t*)&op->status, ASYNC_STATUS_PENDING, MEMORY_ORDER_RELEASE);
    
    // Copy parameters
    if (params && param_size > 0) {
        op->params = malloc(param_size);
        if (!op->params) {
            return 0;
        }
        memcpy(op->params, params, param_size);
        op->param_size = param_size;
    }

    op->callback = callback;
    op->callback_context = context;
    atomic_store_explicit_ptr(&op->next, NULL, MEMORY_ORDER_RELEASE);
    op->deadline = deadline;
    op->priority = priority;
    atomic_store_explicit_bool(&op->cancellable, true, MEMORY_ORDER_RELEASE);

    // Select worker and add to queue
    worker_state_t* worker = select_worker(type, priority);
    mutex_lock(worker->queue.lock);

    if (!atomic_load_explicit_ptr(&worker->queue.tail, MEMORY_ORDER_ACQUIRE)) {
        atomic_store_explicit_ptr(&worker->queue.head, op, MEMORY_ORDER_RELEASE);
        atomic_store_explicit_ptr(&worker->queue.tail, op, MEMORY_ORDER_RELEASE);
    } else {
        async_op_t* tail = atomic_load_explicit_ptr(&worker->queue.tail, MEMORY_ORDER_ACQUIRE);
        atomic_store_explicit_ptr(&tail->next, op, MEMORY_ORDER_RELEASE);
        atomic_store_explicit_ptr(&worker->queue.tail, op, MEMORY_ORDER_RELEASE);
    }

    atomic_fetch_add_explicit_u32(&worker->queue.count, 1, MEMORY_ORDER_ACQ_REL);
    mutex_unlock(worker->queue.lock);

    return op->op_id;
}

/* Cancel an async operation */
bool async_cancel(uint32_t op_id) {
    for (uint32_t i = 0; i < MAX_PENDING_OPS; i++) {
        async_op_t* op = &async_state.op_pool[i];
        if (op->op_id == op_id && 
            atomic_load_explicit_bool(&op->cancellable, MEMORY_ORDER_ACQUIRE)) {
            atomic_store_explicit_u32((atomic_uint32_t*)&op->status,
                                    ASYNC_STATUS_CANCELLED, MEMORY_ORDER_RELEASE);
            return true;
        }
    }
    return false;
}

/* Wait for an async operation to complete */
bool async_wait(uint32_t op_id, uint32_t timeout_ms) {
    uint64_t start_time = get_system_time_ms();
    
    while ((get_system_time_ms() - start_time) < timeout_ms) {
        for (uint32_t i = 0; i < MAX_PENDING_OPS; i++) {
            async_op_t* op = &async_state.op_pool[i];
            if (op->op_id == op_id) {
                async_status_t status = atomic_load_explicit_u32(
                    (atomic_uint32_t*)&op->status, MEMORY_ORDER_ACQUIRE);
                if (status == ASYNC_STATUS_COMPLETED || 
                    status == ASYNC_STATUS_FAILED ||
                    status == ASYNC_STATUS_CANCELLED) {
                    return true;
                }
                break;
            }
        }
        thread_yield();
    }
    return false;
}

/* Get operation status */
async_status_t async_get_status(uint32_t op_id) {
    for (uint32_t i = 0; i < MAX_PENDING_OPS; i++) {
        async_op_t* op = &async_state.op_pool[i];
        if (op->op_id == op_id) {
            return atomic_load_explicit_u32(
                (atomic_uint32_t*)&op->status, MEMORY_ORDER_ACQUIRE);
        }
    }
    return ASYNC_STATUS_FAILED;
}

/* Process pending operations (called by worker threads) */
void async_process_ops(void) {
    while (atomic_load_explicit_bool(&async_state.initialized, MEMORY_ORDER_ACQUIRE)) {
        bool processed = false;
        
        // Process operations from all queues
        for (uint32_t i = 0; i < async_state.num_workers; i++) {
            worker_state_t* worker = &async_state.workers[i];
            if (!atomic_load_explicit_bool(&worker->active, MEMORY_ORDER_ACQUIRE)) {
                continue;
            }

            mutex_lock(worker->queue.lock);
            async_op_t* op = atomic_load_explicit_ptr(&worker->queue.head, MEMORY_ORDER_ACQUIRE);
            
            if (op) {
                // Remove from queue
                atomic_store_explicit_ptr(&worker->queue.head,
                    atomic_load_explicit_ptr(&op->next, MEMORY_ORDER_ACQUIRE),
                    MEMORY_ORDER_RELEASE);
                
                if (!atomic_load_explicit_ptr(&worker->queue.head, MEMORY_ORDER_ACQUIRE)) {
                    atomic_store_explicit_ptr(&worker->queue.tail, NULL, MEMORY_ORDER_RELEASE);
                }
                
                atomic_fetch_sub_explicit_u32(&worker->queue.count, 1, MEMORY_ORDER_ACQ_REL);
                mutex_unlock(worker->queue.lock);

                // Process operation
                atomic_store_explicit_u32((atomic_uint32_t*)&op->status,
                                        ASYNC_STATUS_IN_PROGRESS, MEMORY_ORDER_RELEASE);
                atomic_store_explicit_bool(&op->cancellable, false, MEMORY_ORDER_RELEASE);

                bool success = true;
                void* result = NULL;

                switch (op->type) {
                    case ASYNC_OP_TASK_MIGRATION:
                        // Handle task migration
                        success = handle_task_migration(op->params);
                        break;
                        
                    case ASYNC_OP_LOAD_UPDATE:
                        // Handle load update
                        success = handle_load_update(op->params);
                        break;
                        
                    case ASYNC_OP_POLICY_CHANGE:
                        // Handle policy change
                        success = handle_policy_change(op->params);
                        break;
                        
                    case ASYNC_OP_THERMAL_ADJUST:
                        // Handle thermal adjustment
                        success = handle_thermal_adjust(op->params);
                        break;
                        
                    case ASYNC_OP_MEMORY_REBALANCE:
                        // Handle memory rebalancing
                        success = handle_memory_rebalance(op->params);
                        break;
                }

                // Update status and call callback
                atomic_store_explicit_u32((atomic_uint32_t*)&op->status,
                    success ? ASYNC_STATUS_COMPLETED : ASYNC_STATUS_FAILED,
                    MEMORY_ORDER_RELEASE);

                if (op->callback) {
                    op->callback(op->callback_context,
                               success ? ASYNC_STATUS_COMPLETED : ASYNC_STATUS_FAILED,
                               result);
                }

                // Cleanup
                if (op->params) {
                    free(op->params);
                    op->params = NULL;
                }

                processed = true;
            } else {
                mutex_unlock(worker->queue.lock);
            }
        }

        if (!processed) {
            thread_yield();
        }
    }
}

/* Cleanup and shutdown async system */
void async_shutdown(void) {
    if (!atomic_load_explicit_bool(&async_state.initialized, MEMORY_ORDER_ACQUIRE)) {
        return;
    }

    // Stop workers
    for (uint32_t i = 0; i < async_state.num_workers; i++) {
        atomic_store_explicit_bool(&async_state.workers[i].active, false, MEMORY_ORDER_RELEASE);
        if (async_state.workers[i].thread) {
            thread_join(async_state.workers[i].thread);
            async_state.workers[i].thread = NULL;
        }
        if (async_state.workers[i].queue.lock) {
            mutex_destroy(async_state.workers[i].queue.lock);
            async_state.workers[i].queue.lock = NULL;
        }
    }

    // Cleanup remaining operations
    for (uint32_t i = 0; i < MAX_PENDING_OPS; i++) {
        async_op_t* op = &async_state.op_pool[i];
        if (op->params) {
            free(op->params);
            op->params = NULL;
        }
    }

    // Free resources
    if (async_state.op_pool) {
        free(async_state.op_pool);
        async_state.op_pool = NULL;
    }
    
    if (async_state.global_lock) {
        mutex_destroy(async_state.global_lock);
        async_state.global_lock = NULL;
    }

    atomic_store_explicit_bool(&async_state.initialized, false, MEMORY_ORDER_RELEASE);
}
