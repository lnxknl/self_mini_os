#ifndef MINI_RTOS_PROTOCOL_H
#define MINI_RTOS_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "hashtable.h"

/* Protocol Types */
typedef enum {
    PROTO_NONE = 0,
    PROTO_UART,
    PROTO_SPI,
    PROTO_I2C,
    PROTO_CAN,
    PROTO_ETHERNET,
    PROTO_CUSTOM
} protocol_type_t;

/* Protocol States */
typedef enum {
    PROTO_STATE_IDLE = 0,
    PROTO_STATE_BUSY,
    PROTO_STATE_ERROR,
    PROTO_STATE_TIMEOUT
} protocol_state_t;

/* Protocol Frame Structure */
typedef struct {
    uint8_t *data;
    uint32_t length;
    uint32_t max_size;
    uint32_t position;
    uint32_t checksum;
} proto_frame_t;

/* Protocol Handler Structure */
typedef struct {
    protocol_type_t type;
    protocol_state_t state;
    uint32_t timeout_ms;
    uint32_t retry_count;
    uint32_t max_retries;
    void (*init)(void *config);
    bool (*send)(proto_frame_t *frame);
    bool (*receive)(proto_frame_t *frame);
    void (*error_handler)(uint32_t error_code);
    void *config;
} proto_handler_t;

/* Protocol Manager Structure */
typedef struct {
    hashtable_t *handlers;
    uint32_t active_protocols;
    bool initialized;
} proto_manager_t;

/* Protocol Error Codes */
#define PROTO_ERR_NONE          0x00
#define PROTO_ERR_TIMEOUT       0x01
#define PROTO_ERR_CHECKSUM      0x02
#define PROTO_ERR_OVERFLOW      0x03
#define PROTO_ERR_UNDERFLOW     0x04
#define PROTO_ERR_BUSY          0x05
#define PROTO_ERR_NOT_FOUND     0x06
#define PROTO_ERR_INVALID_STATE 0x07

/* Protocol API Functions */
void proto_init(void);
void proto_shutdown(void);
bool proto_register(protocol_type_t type, proto_handler_t *handler);
bool proto_unregister(protocol_type_t type);
proto_handler_t *proto_get_handler(protocol_type_t type);
bool proto_send(protocol_type_t type, proto_frame_t *frame);
bool proto_receive(protocol_type_t type, proto_frame_t *frame);
void proto_process_errors(protocol_type_t type, uint32_t error_code);

/* Frame Management Functions */
proto_frame_t *proto_frame_create(uint32_t max_size);
void proto_frame_destroy(proto_frame_t *frame);
bool proto_frame_reset(proto_frame_t *frame);
bool proto_frame_append(proto_frame_t *frame, const uint8_t *data, uint32_t length);
uint32_t proto_frame_calculate_checksum(const proto_frame_t *frame);
bool proto_frame_validate(const proto_frame_t *frame);

#endif /* MINI_RTOS_PROTOCOL_H */
