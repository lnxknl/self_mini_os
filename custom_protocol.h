#ifndef _CUSTOM_PROTOCOL_H
#define _CUSTOM_PROTOCOL_H

#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <net/sock.h>

/* Protocol Constants */
#define CPROTO_VERSION     1
#define CPROTO_HDR_SIZE    32
#define CPROTO_MAX_WINDOW  65535
#define CPROTO_DEFAULT_MSS 1460
#define CPROTO_MAX_RETRIES 5

/* Protocol Flags */
#define CPROTO_SYN        0x01
#define CPROTO_ACK        0x02
#define CPROTO_FIN        0x04
#define CPROTO_RST        0x08
#define CPROTO_PSH        0x10
#define CPROTO_URG        0x20
#define CPROTO_ECN        0x40
#define CPROTO_CWR        0x80

/* Connection States */
enum cproto_state {
    CPROTO_CLOSED,
    CPROTO_LISTEN,
    CPROTO_SYN_SENT,
    CPROTO_SYN_RECEIVED,
    CPROTO_ESTABLISHED,
    CPROTO_FIN_WAIT_1,
    CPROTO_FIN_WAIT_2,
    CPROTO_CLOSE_WAIT,
    CPROTO_CLOSING,
    CPROTO_LAST_ACK,
    CPROTO_TIME_WAIT
};

/* Flow Control */
struct cproto_flow_control {
    __u32 cwnd;                  /* Congestion window */
    __u32 ssthresh;             /* Slow start threshold */
    __u32 rtt;                  /* Round trip time */
    __u32 rtt_var;             /* RTT variance */
    __u32 rto;                  /* Retransmission timeout */
    __u16 mss;                  /* Maximum segment size */
    __u8  retries;             /* Current retry count */
    __u8  reserved;            /* Padding */
};

/* Protocol Header */
struct cproto_hdr {
    __be16 source;              /* Source port */
    __be16 dest;                /* Destination port */
    __be32 seq;                 /* Sequence number */
    __be32 ack_seq;            /* Acknowledgment number */
    __u8   version;            /* Protocol version */
    __u8   flags;              /* Control flags */
    __be16 window;             /* Window size */
    __be16 checksum;           /* Checksum */
    __be16 urg_ptr;            /* Urgent pointer */
    __be32 options;            /* Optional fields */
};

/* Socket Structure */
struct cproto_sock {
    struct sock sk;             /* Inherited sock structure */
    struct cproto_flow_control flow;  /* Flow control info */
    
    /* Connection Info */
    enum cproto_state state;    /* Current connection state */
    __u32 local_seq;           /* Local sequence number */
    __u32 remote_seq;          /* Remote sequence number */
    
    /* Buffers */
    struct sk_buff_head out_queue;  /* Outgoing packet queue */
    struct sk_buff_head in_queue;   /* Incoming packet queue */
    
    /* Timers */
    struct timer_list retrans_timer;  /* Retransmission timer */
    struct timer_list keepalive_timer; /* Keepalive timer */
    struct timer_list fin_timer;      /* FIN timer */
    
    /* Statistics */
    struct {
        __u32 packets_sent;
        __u32 packets_received;
        __u32 retransmissions;
        __u32 checksum_errors;
        __u32 out_of_order;
    } stats;
};

/* Protocol Operations */
struct proto cproto_proto;

/* Function Declarations */

/* Socket Operations */
int cproto_init_sock(struct sock *sk);
void cproto_destroy_sock(struct sock *sk);
int cproto_connect(struct sock *sk, struct sockaddr *addr, int addr_len);
int cproto_disconnect(struct sock *sk, int flags);
int cproto_accept(struct sock *sk, struct sock *newsk, int flags);
int cproto_listen(struct sock *sk, int backlog);

/* Packet Handling */
int cproto_sendmsg(struct sock *sk, struct msghdr *msg, size_t size);
int cproto_recvmsg(struct sock *sk, struct msghdr *msg, size_t size, int flags);
int cproto_parse_header(struct sk_buff *skb, struct cproto_hdr *hdr);
void cproto_build_header(struct cproto_hdr *hdr, struct cproto_sock *cp);

/* Flow Control */
void cproto_init_flow_control(struct cproto_sock *cp);
void cproto_update_window(struct cproto_sock *cp, __u16 window);
void cproto_update_rtt(struct cproto_sock *cp, __u32 rtt_sample);
int cproto_can_send(struct cproto_sock *cp);

/* Timer Management */
void cproto_reset_timers(struct cproto_sock *cp);
void cproto_start_timer(struct timer_list *timer, unsigned long expires);
void cproto_stop_timer(struct timer_list *timer);

/* Utility Functions */
__be16 cproto_checksum(struct sk_buff *skb);
void cproto_update_stats(struct cproto_sock *cp, int event);
int cproto_handle_options(struct cproto_sock *cp, __be32 options);

/* Debug Functions */
void cproto_dump_header(struct cproto_hdr *hdr);
void cproto_dump_sock(struct cproto_sock *cp);

#endif /* _CUSTOM_PROTOCOL_H */
