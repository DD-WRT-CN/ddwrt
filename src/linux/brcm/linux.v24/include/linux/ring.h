/*
 * Definitions for packet ring
 *
 * 2004 - Luca Deri <deri@ntop.org>
 */
#ifndef __RING_H
#define __RING_H


#define INCLUDE_MAC_INFO

#ifdef INCLUDE_MAC_INFO
#define SKB_DISPLACEMENT    14 /* Include MAC address information */
#else
#define SKB_DISPLACEMENT    0  /* Do NOT include MAC address information */
#endif

#define RING_MAGIC
#define RING_MAGIC_VALUE      0x88
#define RING_FLOWSLOT_VERSION 4

/* *********************************** */

#ifndef HAVE_PCAP
struct pcap_pkthdr {
  struct timeval ts;    /* time stamp */
  u_int32_t caplen;     /* length of portion present */
  u_int32_t len;        /* length this packet (off wire) */
};
#endif

/* *********************************** */

#define RING_MIN_SLOT_SIZE    (60+sizeof(struct pcap_pkthdr))
#define RING_MAX_SLOT_SIZE    (1514+sizeof(struct pcap_pkthdr))

/* *********************************** */

typedef struct flowSlotInfo {
  u_int16_t version, sample_rate;
  u_int32_t tot_slots, slot_len, tot_mem;
  
  u_int32_t tot_pkts, tot_lost;
  u_int32_t tot_insert, tot_read;  
  u_int16_t insert_idx;
  u_int16_t remove_idx;
} FlowSlotInfo;

/* *********************************** */

typedef struct flowSlot {
#ifdef RING_MAGIC
  u_char     magic;      /* It must alwasy be zero */
#endif
  u_char     slot_state; /* 0=empty, 1=full   */
  u_char     bucket;     /* bucket[bucketLen] */
} FlowSlot;

/* *********************************** */

#ifdef __KERNEL__ 
FlowSlotInfo* getRingPtr(void);
int allocateRing(char *deviceName, u_int numSlots, u_int bucketLen, u_int sampleRate);
unsigned int pollRing(struct file *fp, struct poll_table_struct * wait);
void deallocateRing(void);

/* ************************* */

typedef int (*handle_ring_skb)(struct sk_buff *skb, u_char recv_packet);
extern void set_ring_handler(handle_ring_skb the_handler);

#endif /* __KERNEL__  */

/* *********************************** */

#define PF_RING          27      /* Packet Ring */
#define SOCK_RING        PF_RING

/* ioctl() */
#define SIORINGPOLL      0x8888

/* *********************************** */

#endif /* __RING_H */
