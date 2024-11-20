#ifndef IPC_H
#define IPC_H

#include "rtos_types.h"
#include <stdint.h>

/* Message Types */
typedef enum {
    MSG_NORMAL = 0,
    MSG_HIGH_PRIORITY,
    MSG_URGENT,
    MSG_SYSTEM
} message_type_t;

/* Message Structure */
typedef struct {
    message_type_t type;
    uint32_t sender_id;
    uint32_t receiver_id;
    uint32_t msg_id;
    uint32_t length;
    void *data;
} message_t;

/* Message Queue */
typedef struct msg_queue {
    message_t *messages;
    uint32_t capacity;
    uint32_t count;
    uint32_t head;
    uint32_t tail;
    tcb_t *waiting_send;
    tcb_t *waiting_receive;
    mutex_t *lock;
} msg_queue_t;

/* Event Flags */
typedef struct {
    uint32_t flags;
    tcb_t *waiting_tasks;
    mutex_t *lock;
} event_group_t;

/* Event Operations */
#define EVENT_WAIT_ALL      0x01
#define EVENT_WAIT_ANY      0x02
#define EVENT_CLEAR_ON_EXIT 0x04

/* IPC Functions */
msg_queue_t *msg_queue_create(uint32_t capacity);
void msg_queue_delete(msg_queue_t *queue);
int msg_send(msg_queue_t *queue, message_t *msg, uint32_t timeout);
int msg_receive(msg_queue_t *queue, message_t *msg, uint32_t timeout);
int msg_peek(msg_queue_t *queue, message_t *msg);

/* Event Functions */
event_group_t *event_group_create(void);
void event_group_delete(event_group_t *group);
uint32_t event_group_set_bits(event_group_t *group, uint32_t bits);
uint32_t event_group_clear_bits(event_group_t *group, uint32_t bits);
uint32_t event_group_wait_bits(event_group_t *group, uint32_t bits, 
                              uint8_t operation, uint32_t timeout);

/* Broadcast messaging */
int msg_broadcast(message_t *msg, uint32_t timeout);

/* Message Filtering */
typedef bool (*msg_filter_t)(message_t *msg, void *arg);
int msg_receive_filtered(msg_queue_t *queue, message_t *msg, 
                        msg_filter_t filter, void *arg, uint32_t timeout);

#endif /* IPC_H */
