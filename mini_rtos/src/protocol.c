#include "protocol.h"
#include "rtos_core.h"
#include <string.h>

/* Static Protocol Manager Instance */
static proto_manager_t proto_manager = {
    .handlers = NULL,
    .active_protocols = 0,
    .initialized = false
};

/* CRC-32 Lookup Table */
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    /* ... Add complete CRC table here ... */
};

/* Initialize Protocol Manager */
void proto_init(void) {
    if (proto_manager.initialized) return;
    
    enter_critical();
    
    proto_manager.handlers = ht_create(16, ht_hash_int, ht_compare_int);
    proto_manager.active_protocols = 0;
    proto_manager.initialized = true;
    
    exit_critical();
}

/* Shutdown Protocol Manager */
void proto_shutdown(void) {
    if (!proto_manager.initialized) return;
    
    enter_critical();
    
    /* Free all registered handlers */
    ht_destroy(proto_manager.handlers);
    proto_manager.handlers = NULL;
    proto_manager.active_protocols = 0;
    proto_manager.initialized = false;
    
    exit_critical();
}

/* Register Protocol Handler */
bool proto_register(protocol_type_t type, proto_handler_t *handler) {
    if (!proto_manager.initialized || !handler) return false;
    
    enter_critical();
    
    bool success = ht_insert(proto_manager.handlers, 
                           (void*)(uintptr_t)type, 
                           handler);
    if (success) {
        proto_manager.active_protocols++;
        if (handler->init) {
            handler->init(handler->config);
        }
    }
    
    exit_critical();
    return success;
}

/* Unregister Protocol Handler */
bool proto_unregister(protocol_type_t type) {
    if (!proto_manager.initialized) return false;
    
    enter_critical();
    
    bool success = ht_remove(proto_manager.handlers, 
                           (void*)(uintptr_t)type);
    if (success) {
        proto_manager.active_protocols--;
    }
    
    exit_critical();
    return success;
}

/* Get Protocol Handler */
proto_handler_t *proto_get_handler(protocol_type_t type) {
    if (!proto_manager.initialized) return NULL;
    return ht_get(proto_manager.handlers, (void*)(uintptr_t)type);
}

/* Send Protocol Frame */
bool proto_send(protocol_type_t type, proto_frame_t *frame) {
    proto_handler_t *handler = proto_get_handler(type);
    if (!handler || !frame || handler->state == PROTO_STATE_ERROR) {
        return false;
    }
    
    enter_critical();
    
    bool success = false;
    handler->state = PROTO_STATE_BUSY;
    
    /* Calculate checksum before sending */
    frame->checksum = proto_frame_calculate_checksum(frame);
    
    /* Attempt to send with retries */
    uint32_t retries = 0;
    while (retries <= handler->max_retries) {
        if (handler->send(frame)) {
            success = true;
            break;
        }
        retries++;
    }
    
    handler->state = success ? PROTO_STATE_IDLE : PROTO_STATE_ERROR;
    if (!success) {
        handler->error_handler(PROTO_ERR_TIMEOUT);
    }
    
    exit_critical();
    return success;
}

/* Receive Protocol Frame */
bool proto_receive(protocol_type_t type, proto_frame_t *frame) {
    proto_handler_t *handler = proto_get_handler(type);
    if (!handler || !frame || handler->state == PROTO_STATE_ERROR) {
        return false;
    }
    
    enter_critical();
    
    bool success = false;
    handler->state = PROTO_STATE_BUSY;
    
    /* Attempt to receive */
    if (handler->receive(frame)) {
        /* Validate received frame */
        if (proto_frame_validate(frame)) {
            success = true;
        } else {
            handler->error_handler(PROTO_ERR_CHECKSUM);
        }
    }
    
    handler->state = success ? PROTO_STATE_IDLE : PROTO_STATE_ERROR;
    
    exit_critical();
    return success;
}

/* Process Protocol Errors */
void proto_process_errors(protocol_type_t type, uint32_t error_code) {
    proto_handler_t *handler = proto_get_handler(type);
    if (!handler) return;
    
    enter_critical();
    
    handler->state = PROTO_STATE_ERROR;
    if (handler->error_handler) {
        handler->error_handler(error_code);
    }
    
    exit_critical();
}

/* Frame Management Functions */
proto_frame_t *proto_frame_create(uint32_t max_size) {
    proto_frame_t *frame = malloc(sizeof(proto_frame_t));
    if (!frame) return NULL;
    
    frame->data = malloc(max_size);
    if (!frame->data) {
        free(frame);
        return NULL;
    }
    
    frame->length = 0;
    frame->max_size = max_size;
    frame->position = 0;
    frame->checksum = 0;
    
    return frame;
}

void proto_frame_destroy(proto_frame_t *frame) {
    if (!frame) return;
    if (frame->data) free(frame->data);
    free(frame);
}

bool proto_frame_reset(proto_frame_t *frame) {
    if (!frame) return false;
    
    frame->length = 0;
    frame->position = 0;
    frame->checksum = 0;
    
    return true;
}

bool proto_frame_append(proto_frame_t *frame, const uint8_t *data, uint32_t length) {
    if (!frame || !data || frame->length + length > frame->max_size) {
        return false;
    }
    
    memcpy(frame->data + frame->length, data, length);
    frame->length += length;
    
    return true;
}

uint32_t proto_frame_calculate_checksum(const proto_frame_t *frame) {
    if (!frame || !frame->data) return 0;
    
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < frame->length; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ frame->data[i]];
    }
    
    return ~crc;
}

bool proto_frame_validate(const proto_frame_t *frame) {
    if (!frame || !frame->data) return false;
    
    uint32_t calculated_checksum = proto_frame_calculate_checksum(frame);
    return calculated_checksum == frame->checksum;
}
