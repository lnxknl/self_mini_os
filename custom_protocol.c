#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/random.h>
#include "custom_protocol.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Custom Protocol Developer");
MODULE_DESCRIPTION("Custom Reliable Transport Protocol");
MODULE_VERSION("1.0");

/* Protocol initialization */
static struct proto cproto_proto = {
    .name     = "CPROTO",
    .owner    = THIS_MODULE,
    .obj_size = sizeof(struct cproto_sock),
};

/* Socket Operations Implementation */
int cproto_init_sock(struct sock *sk)
{
    struct cproto_sock *cp = (struct cproto_sock *)sk;

    /* Initialize socket structure */
    skb_queue_head_init(&cp->out_queue);
    skb_queue_head_init(&cp->in_queue);
    
    /* Initialize flow control */
    cp->flow.cwnd = CPROTO_DEFAULT_MSS;
    cp->flow.ssthresh = CPROTO_MAX_WINDOW;
    cp->flow.mss = CPROTO_DEFAULT_MSS;
    cp->flow.rto = HZ;  /* 1 second default RTO */
    
    /* Initialize state */
    cp->state = CPROTO_CLOSED;
    get_random_bytes(&cp->local_seq, sizeof(cp->local_seq));
    
    /* Initialize timers */
    timer_setup(&cp->retrans_timer, cproto_retransmit_handler, 0);
    timer_setup(&cp->keepalive_timer, cproto_keepalive_handler, 0);
    timer_setup(&cp->fin_timer, cproto_fin_handler, 0);
    
    return 0;
}

void cproto_destroy_sock(struct sock *sk)
{
    struct cproto_sock *cp = (struct cproto_sock *)sk;
    
    /* Cancel all timers */
    del_timer_sync(&cp->retrans_timer);
    del_timer_sync(&cp->keepalive_timer);
    del_timer_sync(&cp->fin_timer);
    
    /* Clean up queues */
    skb_queue_purge(&cp->out_queue);
    skb_queue_purge(&cp->in_queue);
}

/* Connection Establishment */
int cproto_connect(struct sock *sk, struct sockaddr *addr, int addr_len)
{
    struct cproto_sock *cp = (struct cproto_sock *)sk;
    struct cproto_hdr *hdr;
    struct sk_buff *skb;
    
    /* Allocate and prepare SYN packet */
    skb = alloc_skb(CPROTO_HDR_SIZE, GFP_KERNEL);
    if (!skb)
        return -ENOMEM;
        
    /* Build SYN header */
    hdr = (struct cproto_hdr *)skb_put(skb, CPROTO_HDR_SIZE);
    memset(hdr, 0, CPROTO_HDR_SIZE);
    
    hdr->source = sk->sk_port;
    hdr->dest = ((struct sockaddr_in *)addr)->sin_port;
    hdr->seq = cp->local_seq;
    hdr->flags = CPROTO_SYN;
    hdr->window = CPROTO_MAX_WINDOW;
    hdr->version = CPROTO_VERSION;
    
    /* Calculate checksum */
    hdr->checksum = cproto_checksum(skb);
    
    /* Update state and send packet */
    cp->state = CPROTO_SYN_SENT;
    if (cproto_transmit_skb(sk, skb) < 0) {
        kfree_skb(skb);
        return -ECONNREFUSED;
    }
    
    /* Start retransmission timer */
    cproto_start_timer(&cp->retrans_timer, jiffies + cp->flow.rto);
    
    return 0;
}

/* Packet Processing */
static int cproto_process_packet(struct sock *sk, struct sk_buff *skb)
{
    struct cproto_sock *cp = (struct cproto_sock *)sk;
    struct cproto_hdr *hdr = (struct cproto_hdr *)skb->data;
    
    /* Verify checksum */
    if (cproto_checksum(skb) != 0) {
        cp->stats.checksum_errors++;
        return -EINVAL;
    }
    
    /* Process based on current state */
    switch (cp->state) {
        case CPROTO_CLOSED:
            /* Drop packet in closed state */
            return -ENOTCONN;
            
        case CPROTO_LISTEN:
            if (hdr->flags & CPROTO_SYN)
                return cproto_handle_syn(sk, skb);
            break;
            
        case CPROTO_SYN_SENT:
            if ((hdr->flags & (CPROTO_SYN | CPROTO_ACK)) == 
                (CPROTO_SYN | CPROTO_ACK))
                return cproto_handle_synack(sk, skb);
            break;
            
        case CPROTO_ESTABLISHED:
            return cproto_handle_established(sk, skb);
            
        default:
            /* Handle other states */
            break;
    }
    
    return 0;
}

/* Flow Control */
void cproto_update_rtt(struct cproto_sock *cp, __u32 rtt_sample)
{
    /* Update RTT using EWMA (Exponentially Weighted Moving Average) */
    if (cp->flow.rtt == 0) {
        cp->flow.rtt = rtt_sample;
        cp->flow.rtt_var = rtt_sample / 2;
    } else {
        /* RFC 6298 RTT calculation */
        __u32 delta = rtt_sample - cp->flow.rtt;
        cp->flow.rtt += (delta >> 3);      /* RTT = 7/8 RTT + 1/8 sample */
        if (delta < 0)
            delta = -delta;
        cp->flow.rtt_var += ((delta - cp->flow.rtt_var) >> 2);
        
        /* Update RTO */
        cp->flow.rto = cp->flow.rtt + (cp->flow.rtt_var << 2);
        if (cp->flow.rto < HZ/2)    /* Minimum RTO of 500ms */
            cp->flow.rto = HZ/2;
        if (cp->flow.rto > HZ*60)   /* Maximum RTO of 60 seconds */
            cp->flow.rto = HZ*60;
    }
}

/* Congestion Control */
static void cproto_congestion_event(struct cproto_sock *cp)
{
    /* Implement TCP-like congestion control */
    cp->flow.ssthresh = max(cp->flow.cwnd >> 1, 
                           2 * CPROTO_DEFAULT_MSS);
    cp->flow.cwnd = cp->flow.ssthresh;
}

/* Timer Handlers */
static void cproto_retransmit_handler(struct timer_list *t)
{
    struct cproto_sock *cp = from_timer(cp, t, retrans_timer);
    
    if (cp->flow.retries++ < CPROTO_MAX_RETRIES) {
        /* Retransmit unacked packets */
        struct sk_buff *skb;
        skb_queue_walk(&cp->out_queue, skb) {
            if (!(TCP_SKB_CB(skb)->tcp_flags & CPROTO_ACK)) {
                cproto_transmit_skb((struct sock *)cp, skb);
                break;
            }
        }
        /* Exponential backoff */
        cproto_start_timer(&cp->retrans_timer, 
                          jiffies + (cp->flow.rto << cp->flow.retries));
    } else {
        /* Connection failed */
        cp->state = CPROTO_CLOSED;
    }
}

/* Utility Functions */
__be16 cproto_checksum(struct sk_buff *skb)
{
    struct cproto_hdr *hdr = (struct cproto_hdr *)skb->data;
    __be16 orig_checksum = hdr->checksum;
    __be16 new_checksum;
    
    /* Zero out checksum field for calculation */
    hdr->checksum = 0;
    
    /* Calculate Internet checksum */
    new_checksum = ip_compute_csum(skb->data, skb->len);
    
    /* Restore original checksum */
    hdr->checksum = orig_checksum;
    
    return new_checksum;
}

/* Module Init/Exit */
static int __init cproto_init(void)
{
    int rc;
    
    /* Register protocol */
    rc = proto_register(&cproto_proto, 1);
    if (rc)
        return rc;
        
    /* Register protocol handler */
    rc = inet_add_protocol(&cproto_protocol, IPPROTO_CPROTO);
    if (rc) {
        proto_unregister(&cproto_proto);
        return rc;
    }
    
    printk(KERN_INFO "Custom Protocol (CPROTO) loaded\n");
    return 0;
}

static void __exit cproto_exit(void)
{
    inet_del_protocol(&cproto_protocol, IPPROTO_CPROTO);
    proto_unregister(&cproto_proto);
    printk(KERN_INFO "Custom Protocol (CPROTO) unloaded\n");
}

module_init(cproto_init);
module_exit(cproto_exit);
