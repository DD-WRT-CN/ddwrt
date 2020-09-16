/*
 *
 * (C) 2004-06 - Luca Deri <deri@ntop.org>
 *
 * This code includes contributions courtesy of
 * - Jeff Randall <jrandall@nexvu.com>
 * - Helmut Manck <helmut.manck@secunet.com>
 * - Brad Doctor <brad@stillsecure.com>
 * - Amit D. Chaudhary <amit_ml@rajgad.com>
 * - Francesco Fusco <fusco@ntop.org>
 *
 */

#include <linux/version.h>
#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/socket.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/in6.h>
#include <linux/init.h>
#include <linux/filter.h>
#include <linux/ring.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/list.h>
#include <linux/proc_fs.h> 
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#include <net/xfrm.h>
#else
#include <linux/poll.h>
#endif
#include <net/sock.h>
#include <asm/io.h>   /* needed for virt_to_phys() */
#ifdef CONFIG_INET
#include <net/inet_common.h>
#endif

/* #define RING_DEBUG */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11))
static inline int remap_page_range(struct vm_area_struct *vma,
				   unsigned long uvaddr,
				   unsigned long paddr,
				   unsigned long size,
				   pgprot_t prot) {
  return(remap_pfn_range(vma, uvaddr, paddr >> PAGE_SHIFT,
			 size, prot));
}
#endif

/* ************************************************* */

#define CLUSTER_LEN       8

struct ring_cluster {
  u_short             cluster_id; /* 0 = no cluster */
  u_short             num_cluster_elements;
  enum cluster_type   hashing_mode;
  u_short             hashing_id;
  struct sock         *sk[CLUSTER_LEN];
  struct ring_cluster *next;      /* NULL = last element of the cluster */
};

/* ************************************************* */

struct ring_element {
  struct list_head  list;
  struct sock      *sk;
};

/* ************************************************* */

struct ring_opt {
  struct net_device *ring_netdev;

  u_short ring_id;

  /* Cluster */
  u_short cluster_id; /* 0 = no cluster */

  /* Reflector */
  struct net_device *reflector_dev;

  /* Packet buffers */
  unsigned long order;

  /* Ring Slots */
  unsigned long ring_memory;
  FlowSlotInfo *slots_info; /* Basically it points to ring_memory */
  char *ring_slots;  /* Basically it points to ring_memory
			+sizeof(FlowSlotInfo) */

  /* Packet Sampling */
  u_int pktToSample, sample_rate;

  /* BPF Filter */
  struct sk_filter *bpfFilter;

  /* Locks */
  atomic_t num_ring_slots_waiters;
  wait_queue_head_t ring_slots_waitqueue;
  rwlock_t ring_index_lock;

  /* Indexes (Internal) */
  u_int insert_page_id, insert_slot_id;
};

/* ************************************************* */

/* List of all ring sockets. */
static struct list_head ring_table;
static u_int ring_table_size;

/* List of all clusters */
static struct ring_cluster *ring_cluster_list;

static rwlock_t ring_mgmt_lock = RW_LOCK_UNLOCKED;

/* ********************************** */

/* /proc entry for ring module */
struct proc_dir_entry *ring_proc_dir = NULL;
struct proc_dir_entry *ring_proc = NULL;

static int ring_proc_get_info(char *, char **, off_t, int, int *, void *);
static void ring_proc_add(struct ring_opt *pfr);
static void ring_proc_remove(struct ring_opt *pfr);
static void ring_proc_init(void);
static void ring_proc_term(void);
static int ring_count = 0;

/* ********************************** */

/* Forward */
static struct proto_ops ring_ops;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11))
static struct proto ring_proto;
#endif

static int skb_ring_handler(struct sk_buff *skb, u_char recv_packet,
			    u_char real_skb);
static int buffer_ring_handler(struct net_device *dev, char *data, int len);
static int remove_from_cluster(struct sock *sock, struct ring_opt *pfr);

/* Extern */

/* ********************************** */

/* Defaults */
static u_int bucket_len = 128, num_slots = 4096, sample_rate = 1,
  transparent_mode = 1, enable_tx_capture = 0;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
module_param(bucket_len, int, 128);
//module_param(num_slots,  int, 4096);
module_param(sample_rate, int, 1);
module_param(transparent_mode, int, 1);
module_param(enable_tx_capture, int, 0);
#else
MODULE_PARM(bucket_len, "i");
MODULE_PARM(num_slots, "i");
MODULE_PARM(sample_rate, "i");
MODULE_PARM(transparent_mode, "i");
MODULE_PARM(enable_tx_capture, "i");
#endif

MODULE_PARM_DESC(bucket_len, "Number of ring buckets");
MODULE_PARM_DESC(num_slots,  "Number of ring slots");
MODULE_PARM_DESC(sample_rate, "Ring packet sample rate");
MODULE_PARM_DESC(transparent_mode,
		 "Set to 1 to set transparent mode "
		 "(slower but backwards compatible)");

MODULE_PARM_DESC(enable_tx_capture, "Set to 1 to capture outgoing packets");

/* ********************************** */

#define MIN_QUEUED_PKTS      64
#define MAX_QUEUE_LOOPS      64


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#define ring_sk_datatype(__sk) ((struct ring_opt *)__sk)
#define ring_sk(__sk) ((__sk)->sk_protinfo)
#else
#define ring_sk_datatype(a) (a)
#define ring_sk(__sk) ((__sk)->protinfo.pf_ring)
#endif

#define _rdtsc() ({ uint64_t x; asm volatile("rdtsc" : "=A" (x)); x; })

/*
  int dev_queue_xmit(struct sk_buff *skb)
  skb->dev;
  struct net_device *dev_get_by_name(const char *name)
*/

/* ********************************** */

static void ring_sock_destruct(struct sock *sk) {

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
  skb_queue_purge(&sk->sk_receive_queue);

  if (!sock_flag(sk, SOCK_DEAD)) {
#if defined(RING_DEBUG)
    printk("Attempt to release alive ring socket: %p\n", sk);
#endif
    return;
  }

  BUG_TRAP(!atomic_read(&sk->sk_rmem_alloc));
  BUG_TRAP(!atomic_read(&sk->sk_wmem_alloc));
#else

  BUG_TRAP(atomic_read(&sk->rmem_alloc)==0);
  BUG_TRAP(atomic_read(&sk->wmem_alloc)==0);

  if (!sk->dead) {
#if defined(RING_DEBUG)
    printk("Attempt to release alive ring socket: %p\n", sk);
#endif
    return;
  }
#endif

  kfree(ring_sk(sk));

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
  MOD_DEC_USE_COUNT;
#endif
}

/* ********************************** */

static void ring_proc_add(struct ring_opt *pfr) {
  if(ring_proc_dir != NULL) {
    char name[16];

    pfr->ring_id = ring_count++;

    snprintf(name, sizeof(name), "%d", pfr->ring_id);
    create_proc_read_entry(name, 0, ring_proc_dir, 
			   ring_proc_get_info, pfr);
    /* printk("PF_RING: added /proc/net/pf_ring/%s\n", name); */
  }
}

/* ********************************** */

static void ring_proc_remove(struct ring_opt *pfr) {
  if(ring_proc_dir != NULL) {
    char name[16];
    
    snprintf(name, sizeof(name), "%d", pfr->ring_id);    
    remove_proc_entry(name, ring_proc_dir);
    /* printk("PF_RING: removed /proc/net/pf_ring/%s\n", name); */
  }
}

/* ********************************** */

static int ring_proc_get_info(char *buf, char **start, off_t offset,
			      int len, int *unused, void *data)
{
  int rlen = 0;
  struct ring_opt *pfr;
  FlowSlotInfo *fsi;

  if(data == NULL) {
    /* /proc/net/pf_ring/info */
    rlen = sprintf(buf,"Version       : %s\n", RING_VERSION);
    rlen += sprintf(buf + rlen,"Bucket length : %d bytes\n", bucket_len);
    rlen += sprintf(buf + rlen,"Ring slots    : %d\n", num_slots);
    rlen += sprintf(buf + rlen,"Sample rate   : %d [1=no sampling]\n", sample_rate);

    rlen += sprintf(buf + rlen,"Capture TX    : %s\n",
		    enable_tx_capture ? "Yes [RX+TX]" : "No [RX only]");
    rlen += sprintf(buf + rlen,"Total rings   : %d\n", ring_table_size);
  } else {
    /* detailed statistics about a PF_RING */
    pfr = (struct ring_opt*)data;

    if(data) {
      fsi = pfr->slots_info;

      if(fsi) {
	rlen = sprintf(buf,        "Bound Device  : %s\n",
		       pfr->ring_netdev->name == NULL ? "<NULL>" : pfr->ring_netdev->name);
	rlen += sprintf(buf + rlen,"Version       : %d\n",  fsi->version);
	rlen += sprintf(buf + rlen,"Sampling Rate : %d\n",  pfr->sample_rate);
	rlen += sprintf(buf + rlen,"Cluster Id    : %d\n",  pfr->cluster_id);
	rlen += sprintf(buf + rlen,"Tot Slots     : %d\n",  fsi->tot_slots);
	rlen += sprintf(buf + rlen,"Slot Len      : %d\n",  fsi->slot_len);
	rlen += sprintf(buf + rlen,"Data Len      : %d\n",  fsi->data_len);
	rlen += sprintf(buf + rlen,"Tot Memory    : %d\n",  fsi->tot_mem);
	rlen += sprintf(buf + rlen,"Tot Packets   : %lu\n", (unsigned long)fsi->tot_pkts);
	rlen += sprintf(buf + rlen,"Tot Pkt Lost  : %lu\n", (unsigned long)fsi->tot_lost);
	rlen += sprintf(buf + rlen,"Tot Insert    : %lu\n", (unsigned long)fsi->tot_insert);
	rlen += sprintf(buf + rlen,"Tot Read      : %lu\n", (unsigned long)fsi->tot_read);
	
      } else
	rlen = sprintf(buf, "WARNING fsi == NULL\n");
    } else
      rlen = sprintf(buf, "WARNING data == NULL\n");
  }

  return rlen;
}

/* ********************************** */

static void ring_proc_init(void) {
  ring_proc_dir = proc_mkdir("pf_ring", proc_net);
  
  if(ring_proc_dir) {
    ring_proc_dir->owner = THIS_MODULE;
    ring_proc = create_proc_read_entry("info", 0, ring_proc_dir,
				       ring_proc_get_info, NULL);
    if(!ring_proc)
      printk("PF_RING: unable to register proc file\n");
    else {
      ring_proc->owner = THIS_MODULE;
      printk("PF_RING: registered /proc/net/pf_ring/\n");
    }    
  } else
    printk("PF_RING: unable to create /proc/net/pf_ring\n");
}

/* ********************************** */

static void ring_proc_term(void) {
  if(ring_proc != NULL) {
    remove_proc_entry("info", ring_proc_dir);
    if(ring_proc_dir != NULL) remove_proc_entry("pf_ring", proc_net);

    printk("PF_RING: deregistered /proc/net/pf_ring\n");
  }
}

/* ********************************** */

/*
 * ring_insert()
 *
 * store the sk in a new element and add it
 * to the head of the list.
 */
static inline void ring_insert(struct sock *sk) {
  struct ring_element *next;

#if defined(RING_DEBUG)
  printk("RING: ring_insert()\n");
#endif

  next = kmalloc(sizeof(struct ring_element), GFP_ATOMIC);
  if(next != NULL) {
    next->sk = sk;
    write_lock_irq(&ring_mgmt_lock);
    list_add(&next->list, &ring_table);
    write_unlock_irq(&ring_mgmt_lock);
  } else {
    if(net_ratelimit())
      printk("RING: could not kmalloc slot!!\n");
  }

  ring_table_size++;
  ring_proc_add(ring_sk(sk));  
}

/* ********************************** */

/*
 * ring_remove()
 *
 * For each of the elements in the list:
 *  - check if this is the element we want to delete
 *  - if it is, remove it from the list, and free it.
 *
 * stop when we find the one we're looking for (break),
 * or when we reach the end of the list.
 */
static inline void ring_remove(struct sock *sk) {
  struct list_head *ptr;
  struct ring_element *entry;

  for(ptr = ring_table.next; ptr != &ring_table; ptr = ptr->next) {
    entry = list_entry(ptr, struct ring_element, list);

    if(entry->sk == sk) {
      list_del(ptr);
      kfree(ptr);
      ring_table_size--;
      break;
    }
  }
}

/* ********************************** */

static u_int32_t num_queued_pkts(struct ring_opt *pfr) {

  if(pfr->ring_slots != NULL) {

    u_int32_t tot_insert = pfr->slots_info->insert_idx,
#if defined(RING_DEBUG)
      tot_read = pfr->slots_info->tot_read, tot_pkts;
#else
    tot_read = pfr->slots_info->tot_read;
#endif

    if(tot_insert >= tot_read) {
#if defined(RING_DEBUG)
      tot_pkts = tot_insert-tot_read;
#endif
      return(tot_insert-tot_read);
    } else {
#if defined(RING_DEBUG)
      tot_pkts = ((u_int32_t)-1)+tot_insert-tot_read;
#endif
      return(((u_int32_t)-1)+tot_insert-tot_read);
    }

#if defined(RING_DEBUG)
    printk("-> num_queued_pkts=%d [tot_insert=%d][tot_read=%d]\n",
	   tot_pkts, tot_insert, tot_read);
#endif

  } else
    return(0);
}

/* ********************************** */

static inline FlowSlot* get_insert_slot(struct ring_opt *pfr) {
#if defined(RING_DEBUG)
  printk("get_insert_slot(%d)\n", pfr->slots_info->insert_idx);
#endif

  if(pfr->ring_slots != NULL) {
    FlowSlot *slot = (FlowSlot*)&(pfr->ring_slots[pfr->slots_info->insert_idx
						  *pfr->slots_info->slot_len]);
    return(slot);
  } else
    return(NULL);
}

/* ********************************** */

static inline FlowSlot* get_remove_slot(struct ring_opt *pfr) {
#if defined(RING_DEBUG)
  printk("get_remove_slot(%d)\n", pfr->slots_info->remove_idx);
#endif

  if(pfr->ring_slots != NULL)
    return((FlowSlot*)&(pfr->ring_slots[pfr->slots_info->remove_idx*
					pfr->slots_info->slot_len]));
  else
    return(NULL);
}

/* ********************************** */

static void add_skb_to_ring(struct sk_buff *skb,
			    struct ring_opt *pfr,
			    u_char recv_packet,
			    u_char real_skb /* 1=skb 0=faked skb */) {
  FlowSlot *theSlot;
  int idx, displ;

  if(recv_packet) {
    /* Hack for identifying a packet received by the e1000 */
    if(real_skb) {
      displ = SKB_DISPLACEMENT;
    } else
      displ = 0; /* Received by the e1000 wrapper */
  } else
    displ = 0;

  write_lock(&pfr->ring_index_lock);
  pfr->slots_info->tot_pkts++;
  write_unlock(&pfr->ring_index_lock);

  /* BPF Filtering (from af_packet.c) */
  if(pfr->bpfFilter != NULL) {
    unsigned res = 1, len;

    len = skb->len-skb->data_len;

    write_lock(&pfr->ring_index_lock);
    skb->data -= displ;
    res = sk_run_filter(skb, pfr->bpfFilter->insns, pfr->bpfFilter->len);
    skb->data += displ;
    write_unlock(&pfr->ring_index_lock);

    if(res == 0) {
      /* Filter failed */

#if defined(RING_DEBUG)
      printk("add_skb_to_ring(skb): Filter failed [len=%d][tot=%llu]"
	     "[insertIdx=%d][pkt_type=%d][cloned=%d]\n",
	     (int)skb->len, pfr->slots_info->tot_pkts,
	     pfr->slots_info->insert_idx,
	     skb->pkt_type, skb->cloned);
#endif

      return;
    }
  }

  /* ************************** */

  if(pfr->sample_rate > 1) {
    if(pfr->pktToSample == 0) {
      write_lock(&pfr->ring_index_lock);
      pfr->pktToSample = pfr->sample_rate;
      write_unlock(&pfr->ring_index_lock);
    } else {
      write_lock(&pfr->ring_index_lock);
      pfr->pktToSample--;
      write_unlock(&pfr->ring_index_lock);

#if defined(RING_DEBUG)
      printk("add_skb_to_ring(skb): sampled packet [len=%d]"
	     "[tot=%llu][insertIdx=%d][pkt_type=%d][cloned=%d]\n",
	     (int)skb->len, pfr->slots_info->tot_pkts,
	     pfr->slots_info->insert_idx,
	     skb->pkt_type, skb->cloned);
#endif
      return;
    }
  }

  /* ************************************* */

  if((pfr->reflector_dev != NULL)
     && (!netif_queue_stopped(pfr->reflector_dev))) {
    int cpu = smp_processor_id();

    /* increase reference counter so that this skb is not freed */
    atomic_inc(&skb->users);

    skb->data -= displ;

    /* send it */
    if (pfr->reflector_dev->xmit_lock_owner != cpu) {
      spin_lock_bh(&pfr->reflector_dev->_xmit_lock);
      pfr->reflector_dev->xmit_lock_owner = cpu;
      spin_unlock_bh(&pfr->reflector_dev->_xmit_lock);

      if (pfr->reflector_dev->hard_start_xmit(skb,
					      pfr->reflector_dev) == 0) {
        spin_lock_bh(&pfr->reflector_dev->_xmit_lock);
	pfr->reflector_dev->xmit_lock_owner = -1;
	skb->data += displ;
	spin_unlock_bh(&pfr->reflector_dev->_xmit_lock);
#if defined(RING_DEBUG)
	printk("++ hard_start_xmit succeeded\n");
#endif
	return; /* OK */
      }

      spin_lock_bh(&pfr->reflector_dev->_xmit_lock);
      pfr->reflector_dev->xmit_lock_owner = -1;
      spin_unlock_bh(&pfr->reflector_dev->_xmit_lock);
    }

#if defined(RING_DEBUG)
    printk("++ hard_start_xmit failed\n");
#endif
    skb->data += displ;
    return; /* -ENETDOWN */
  }

  /* ************************************* */

#if defined(RING_DEBUG)
  printk("add_skb_to_ring(skb) [len=%d][tot=%llu][insertIdx=%d]"
	 "[pkt_type=%d][cloned=%d]\n",
	 (int)skb->len, pfr->slots_info->tot_pkts,
	 pfr->slots_info->insert_idx,
	 skb->pkt_type, skb->cloned);
#endif

  idx = pfr->slots_info->insert_idx;
  theSlot = get_insert_slot(pfr);

  if((theSlot != NULL) && (theSlot->slot_state == 0)) {
    struct pcap_pkthdr *hdr;
    char *bucket;

    /* Update Index */
    idx++;

    if(idx == pfr->slots_info->tot_slots) {
      write_lock(&pfr->ring_index_lock);
      pfr->slots_info->insert_idx = 0;
      write_unlock(&pfr->ring_index_lock);
    } else {
      write_lock(&pfr->ring_index_lock);
      pfr->slots_info->insert_idx = idx;
      write_unlock(&pfr->ring_index_lock);
    }

    bucket = &theSlot->bucket;
    hdr = (struct pcap_pkthdr*)bucket;

    /* BD - API changed for time keeping */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14))
    if(skb->stamp.tv_sec == 0) do_gettimeofday(&skb->stamp);

    hdr->ts.tv_sec = skb->stamp.tv_sec, hdr->ts.tv_usec = skb->stamp.tv_usec;
#else
    if(skb->tstamp.tv64 == 0) __net_timestamp(skb);

    struct timeval tv = ktime_to_timeval(skb->tstamp);
    hdr->ts.tv_sec = tv.tv_sec, hdr->ts.tv_usec = tv.tv_usec;
#endif

    hdr->caplen    = skb->len+displ;

    if(hdr->caplen > pfr->slots_info->data_len)
      hdr->caplen = pfr->slots_info->data_len;

    hdr->len = skb->len+displ;
    memcpy(&bucket[sizeof(struct pcap_pkthdr)],
	   skb->data-displ, hdr->caplen);

#if defined(RING_DEBUG)
    {
      static unsigned int lastLoss = 0;

      if(pfr->slots_info->tot_lost
	 && (lastLoss != pfr->slots_info->tot_lost)) {
	printk("add_skb_to_ring(%d): [data_len=%d]"
	       "[hdr.caplen=%d][skb->len=%d]"
	       "[pcap_pkthdr=%d][removeIdx=%d]"
	       "[loss=%lu][page=%u][slot=%u]\n",
	       idx-1, pfr->slots_info->data_len, hdr->caplen, skb->len,
	       sizeof(struct pcap_pkthdr),
	       pfr->slots_info->remove_idx,
	       (long unsigned int)pfr->slots_info->tot_lost,
	       pfr->insert_page_id, pfr->insert_slot_id);

	lastLoss = pfr->slots_info->tot_lost;
      }
    }
#endif

    write_lock(&pfr->ring_index_lock);
    pfr->slots_info->tot_insert++;
    theSlot->slot_state = 1;
    write_unlock(&pfr->ring_index_lock);
  } else {
    write_lock(&pfr->ring_index_lock);
    pfr->slots_info->tot_lost++;
    write_unlock(&pfr->ring_index_lock);

#if defined(RING_DEBUG)
    printk("add_skb_to_ring(skb): packet lost [loss=%lu]"
	   "[removeIdx=%u][insertIdx=%u]\n",
	   (long unsigned int)pfr->slots_info->tot_lost,
	   pfr->slots_info->remove_idx, pfr->slots_info->insert_idx);
#endif
  }

  /* wakeup in case of poll() */
  if(waitqueue_active(&pfr->ring_slots_waitqueue))
    wake_up_interruptible(&pfr->ring_slots_waitqueue);
}

/* ********************************** */

static u_int hash_skb(struct ring_cluster *cluster_ptr,
		      struct sk_buff *skb, u_char recv_packet) {
  u_int idx;
  int displ;
  struct iphdr *ip;

  if(cluster_ptr->hashing_mode == cluster_round_robin) {
    idx = cluster_ptr->hashing_id++;
  } else {
    /* Per-flow clustering */
    if(skb->len > sizeof(struct iphdr)+sizeof(struct tcphdr)) {
      if(recv_packet)
	displ = 0;
      else
	displ = SKB_DISPLACEMENT;

      /*
	skb->data+displ

	Always points to to the IP part of the packet
      */

      ip = (struct iphdr*)(skb->data+displ);

      idx = ip->saddr+ip->daddr+ip->protocol;

      if(ip->protocol == IPPROTO_TCP) {
	struct tcphdr *tcp = (struct tcphdr*)(skb->data+displ
					      +sizeof(struct iphdr));
	idx += tcp->source+tcp->dest;
      } else if(ip->protocol == IPPROTO_UDP) {
	struct udphdr *udp = (struct udphdr*)(skb->data+displ
					      +sizeof(struct iphdr));
	idx += udp->source+udp->dest;
      }
    } else
      idx = skb->len;
  }

  return(idx % cluster_ptr->num_cluster_elements);
}

/* ********************************** */

static int skb_ring_handler(struct sk_buff *skb,
			    u_char recv_packet,
			    u_char real_skb /* 1=skb 0=faked skb */) {
  struct sock *skElement;
  int rc = 0;
  struct list_head *ptr;
  struct ring_cluster *cluster_ptr;

#ifdef PROFILING
  uint64_t rdt = _rdtsc(), rdt1, rdt2;
#endif

  if((!skb) /* Invalid skb */
     || ((!enable_tx_capture) && (!recv_packet))) {
    /*
      An outgoing packet is about to be sent out
      but we decided not to handle transmitted
      packets.
    */
    return(0);
  }

#if defined(RING_DEBUG)
  if(0) {
    printk("skb_ring_handler() [len=%d][dev=%s]\n", skb->len,
	   skb->dev->name == NULL ? "<NULL>" : skb->dev->name);
  }
#endif

#ifdef PROFILING
  rdt1 = _rdtsc();
#endif

  /* [1] Check unclustered sockets */
  for (ptr = ring_table.next; ptr != &ring_table; ptr = ptr->next) {
    struct ring_opt *pfr;
    struct ring_element *entry;

    entry = list_entry(ptr, struct ring_element, list);

    read_lock(&ring_mgmt_lock);
    skElement = entry->sk;
    pfr = ring_sk(skElement);
    read_unlock(&ring_mgmt_lock);

    if((pfr != NULL)
       && (pfr->cluster_id == 0 /* No cluster */)
       && (pfr->ring_slots != NULL)
       && (pfr->ring_netdev == skb->dev)) {
      /* We've found the ring where the packet can be stored */
      read_lock(&ring_mgmt_lock);
      add_skb_to_ring(skb, pfr, recv_packet, real_skb);
      read_unlock(&ring_mgmt_lock);

      rc = 1; /* Ring found: we've done our job */
    }
  }

  /* [2] Check socket clusters */
  cluster_ptr = ring_cluster_list;

  while(cluster_ptr != NULL) {
    struct ring_opt *pfr;

    if(cluster_ptr->num_cluster_elements > 0) {
      u_int skb_hash = hash_skb(cluster_ptr, skb, recv_packet);

      read_lock(&ring_mgmt_lock);
      skElement = cluster_ptr->sk[skb_hash];
      read_unlock(&ring_mgmt_lock);

      if(skElement != NULL) {
	pfr = ring_sk(skElement);

	if((pfr != NULL)
	   && (pfr->ring_slots != NULL)
	   && (pfr->ring_netdev == skb->dev)) {
	  /* We've found the ring where the packet can be stored */
          read_lock(&ring_mgmt_lock);
	  add_skb_to_ring(skb, pfr, recv_packet, real_skb);
          read_unlock(&ring_mgmt_lock);

	  rc = 1; /* Ring found: we've done our job */
	}
      }
    }

    cluster_ptr = cluster_ptr->next;
  }

#ifdef PROFILING
  rdt1 = _rdtsc()-rdt1;
#endif

#ifdef PROFILING
  rdt2 = _rdtsc();
#endif

  if(transparent_mode) rc = 0;

  if((rc != 0) && real_skb)
    dev_kfree_skb(skb); /* Free the skb */

#ifdef PROFILING
  rdt2 = _rdtsc()-rdt2;
  rdt = _rdtsc()-rdt;

#if defined(RING_DEBUG)
  printk("# cycles: %d [lock costed %d %d%%][free costed %d %d%%]\n",
	 (int)rdt, rdt-rdt1,
	 (int)((float)((rdt-rdt1)*100)/(float)rdt),
	 rdt2,
	 (int)((float)(rdt2*100)/(float)rdt));
#endif
#endif

  return(rc); /*  0 = packet not handled */
}

/* ********************************** */

struct sk_buff skb;

static int buffer_ring_handler(struct net_device *dev,
			       char *data, int len) {

#if defined(RING_DEBUG)
  printk("buffer_ring_handler: [dev=%s][len=%d]\n",
	 dev->name == NULL ? "<NULL>" : dev->name, len);
#endif

  /* BD - API changed for time keeping */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14))
  skb.dev = dev, skb.len = len, skb.data = data,
    skb.data_len = len, skb.stamp.tv_sec = 0; /* Calculate the time */
#else
struct timeval tv = ktime_to_timeval(skb.tstamp);
tv.tv_sec=0;

  skb.dev = dev, skb.len = len, skb.data = data,
    skb.data_len = len, skb.tstamp = timeval_to_ktime(tv); /* Calculate the time */
#endif

  skb_ring_handler(&skb, 1, 0 /* fake skb */);

  return(0);
}

/* ********************************** */

static int ring_create(struct socket *sock, int protocol) {
  struct sock *sk;
  struct ring_opt *pfr;
  int err;

#if defined(RING_DEBUG)
  printk("RING: ring_create()\n");
#endif

  /* Are you root, superuser or so ? */
  if(!capable(CAP_NET_ADMIN))
    return -EPERM;

  if(sock->type != SOCK_RAW)
    return -ESOCKTNOSUPPORT;

  if(protocol != htons(ETH_P_ALL))
    return -EPROTONOSUPPORT;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
  MOD_INC_USE_COUNT;
#endif

  err = -ENOMEM;

  // BD: -- broke this out to keep it more simple and clear as to what the
  // options are.
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11))
  sk = sk_alloc(PF_RING, GFP_KERNEL, 1, NULL);
#else
  // BD: API changed in 2.6.12, ref:
  // http://svn.clkao.org/svnweb/linux/revision/?rev=28201
  sk = sk_alloc(PF_RING, GFP_ATOMIC, &ring_proto, 1);
#endif
#else
  /* Kernel 2.4 */
  sk = sk_alloc(PF_RING, GFP_KERNEL, 1);
#endif

  if (sk == NULL)
    goto out;

  sock->ops = &ring_ops;
  sock_init_data(sock, sk);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,11))
  sk_set_owner(sk, THIS_MODULE);
#endif
#endif

  err = -ENOMEM;
  ring_sk(sk) = ring_sk_datatype(kmalloc(sizeof(*pfr), GFP_KERNEL));

  if (!(pfr = ring_sk(sk))) {
    sk_free(sk);
    goto out;
  }
  memset(pfr, 0, sizeof(*pfr));
  init_waitqueue_head(&pfr->ring_slots_waitqueue);
  pfr->ring_index_lock = RW_LOCK_UNLOCKED;
  atomic_set(&pfr->num_ring_slots_waiters, 0);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
  sk->sk_family       = PF_RING;
  sk->sk_destruct     = ring_sock_destruct;
#else
  sk->family          = PF_RING;
  sk->destruct        = ring_sock_destruct;
  sk->num             = protocol;
#endif

  ring_insert(sk);

#if defined(RING_DEBUG)
  printk("RING: ring_create() - created\n");
#endif

  return(0);
 out:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
  MOD_DEC_USE_COUNT;
#endif
  return err;
}

/* *********************************************** */

static int ring_release(struct socket *sock)
{
  struct sock *sk = sock->sk;
  struct ring_opt *pfr = ring_sk(sk);

  if(!sk)  return 0;

#if defined(RING_DEBUG)
  printk("RING: called ring_release\n");
#endif

#if defined(RING_DEBUG)
  printk("RING: ring_release entered\n");
#endif

  /* 
     The calls below must be placed outside the
     write_lock_irq...write_unlock_irq block.
  */
  sock_orphan(sk);
  ring_proc_remove(ring_sk(sk));

  write_lock_irq(&ring_mgmt_lock);
  ring_remove(sk);
  sock->sk = NULL;

  /* Free the ring buffer */
  if(pfr->ring_memory) {
    struct page *page, *page_end;

    page_end = virt_to_page(pfr->ring_memory + (PAGE_SIZE << pfr->order) - 1);
    for(page = virt_to_page(pfr->ring_memory); page <= page_end; page++)
      ClearPageReserved(page);

    free_pages(pfr->ring_memory, pfr->order);
  }

  kfree(pfr);
  ring_sk(sk) = NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
  skb_queue_purge(&sk->sk_write_queue);
#endif

  sock_put(sk);
  write_unlock_irq(&ring_mgmt_lock);

#if defined(RING_DEBUG)
  printk("RING: ring_release leaving\n");
#endif

  return 0;
}

/* ********************************** */
/*
 * We create a ring for this socket and bind it to the specified device
 */
static int packet_ring_bind(struct sock *sk, struct net_device *dev)
{
  u_int the_slot_len;
  u_int32_t tot_mem;
  struct ring_opt *pfr = ring_sk(sk);
  struct page *page, *page_end;

  if(!dev) return(-1);

#if defined(RING_DEBUG)
  printk("RING: packet_ring_bind(%s) called\n", dev->name);
#endif

  /* **********************************************

  *************************************
  *                                   *
  *        FlowSlotInfo               *
  *                                   *
  ************************************* <-+
  *        FlowSlot                   *   |
  *************************************   |
  *        FlowSlot                   *   |
  *************************************   +- num_slots
  *        FlowSlot                   *   |
  *************************************   |
  *        FlowSlot                   *   |
  ************************************* <-+

  ********************************************** */

  the_slot_len = sizeof(u_char)    /* flowSlot.slot_state */
#ifdef RING_MAGIC
    + sizeof(u_char)
#endif
    + sizeof(struct pcap_pkthdr)
    + bucket_len      /* flowSlot.bucket */;

  tot_mem = sizeof(FlowSlotInfo) + num_slots*the_slot_len;

  /*
    Calculate the value of the order parameter used later.
    See http://www.linuxjournal.com/article.php?sid=1133
  */
  for(pfr->order = 0;(PAGE_SIZE << pfr->order) < tot_mem; pfr->order++)  ;

  /*
    We now try to allocate the memory as required. If we fail
    we try to allocate a smaller amount or memory (hence a
    smaller ring).
  */
  while((pfr->ring_memory = __get_free_pages(GFP_ATOMIC, pfr->order)) == 0)
    if(pfr->order-- == 0)
      break;

  if(pfr->order == 0) {
    printk("RING: ERROR not enough memory for ring\n");
    return(-1);
  } else {
    printk("RING: succesfully allocated %lu KB [tot_mem=%d][order=%ld]\n",
	   PAGE_SIZE >> (10 - pfr->order), tot_mem, pfr->order);
  }

  tot_mem = PAGE_SIZE << pfr->order;
  memset((char*)pfr->ring_memory, 0, tot_mem);

  /* Now we need to reserve the pages */
  page_end = virt_to_page(pfr->ring_memory + (PAGE_SIZE << pfr->order) - 1);
  for(page = virt_to_page(pfr->ring_memory); page <= page_end; page++)
    SetPageReserved(page);

  pfr->slots_info = (FlowSlotInfo*)pfr->ring_memory;
  pfr->ring_slots = (char*)(pfr->ring_memory+sizeof(FlowSlotInfo));

  pfr->slots_info->version     = RING_FLOWSLOT_VERSION;
  pfr->slots_info->slot_len    = the_slot_len;
  pfr->slots_info->data_len    = bucket_len;
  pfr->slots_info->tot_slots   = (tot_mem-sizeof(FlowSlotInfo))/the_slot_len;
  pfr->slots_info->tot_mem     = tot_mem;
  pfr->slots_info->sample_rate = sample_rate;

  printk("RING: allocated %d slots [slot_len=%d][tot_mem=%u]\n",
	 pfr->slots_info->tot_slots, pfr->slots_info->slot_len,
	 pfr->slots_info->tot_mem);

#ifdef RING_MAGIC
  {
    int i;

    for(i=0; i<pfr->slots_info->tot_slots; i++) {
      unsigned long idx = i*pfr->slots_info->slot_len;
      FlowSlot *slot = (FlowSlot*)&pfr->ring_slots[idx];
      slot->magic = RING_MAGIC_VALUE; slot->slot_state = 0;
    }
  }
#endif

  pfr->insert_page_id = 1, pfr->insert_slot_id = 0;

  /*
    IMPORTANT
    Leave this statement here as last one. In fact when
    the ring_netdev != NULL the socket is ready to be used.
  */
  pfr->ring_netdev = dev;

  return(0);
}

/* ************************************* */

/* Bind to a device */
static int ring_bind(struct socket *sock,
		     struct sockaddr *sa, int addr_len)
{
  struct sock *sk=sock->sk;
  struct net_device *dev = NULL;

#if defined(RING_DEBUG)
  printk("RING: ring_bind() called\n");
#endif

  /*
   *	Check legality
   */
  if (addr_len != sizeof(struct sockaddr))
    return -EINVAL;
  if (sa->sa_family != PF_RING)
    return -EINVAL;

  /* Safety check: add trailing zero if missing */
  sa->sa_data[sizeof(sa->sa_data)-1] = '\0';

#if defined(RING_DEBUG)
  printk("RING: searching device %s\n", sa->sa_data);
#endif

  if((dev = __dev_get_by_name(sa->sa_data)) == NULL) {
#if defined(RING_DEBUG)
    printk("RING: search failed\n");
#endif
    return(-EINVAL);
  } else
    return(packet_ring_bind(sk, dev));
}

/* ************************************* */

static int ring_mmap(struct file *file,
		     struct socket *sock,
		     struct vm_area_struct *vma)
{
  struct sock *sk = sock->sk;
  struct ring_opt *pfr = ring_sk(sk);
  unsigned long size, start;
  u_int pagesToMap;
  char *ptr;

#if defined(RING_DEBUG)
  printk("RING: ring_mmap() called\n");
#endif

  if(pfr->ring_memory == 0) {
#if defined(RING_DEBUG)
    printk("RING: ring_mmap() failed: mapping area to an unbound socket\n");
#endif
    return -EINVAL;
  }

  size = (unsigned long)(vma->vm_end-vma->vm_start);

  if(size % PAGE_SIZE) {
#if defined(RING_DEBUG)
    printk("RING: ring_mmap() failed: len is not multiple of PAGE_SIZE\n");
#endif
    return(-EINVAL);
  }

  /* if userspace tries to mmap beyond end of our buffer, fail */
  if(size > pfr->slots_info->tot_mem) {
#if defined(RING_DEBUG)
    printk("proc_mmap() failed: area too large [%ld > %d]\n", size, pfr->slots_info->tot_mem);
#endif
    return(-EINVAL);
  }

  pagesToMap = size/PAGE_SIZE;

#if defined(RING_DEBUG)
  printk("RING: ring_mmap() called. %d pages to map\n", pagesToMap);
#endif

#if defined(RING_DEBUG)
  printk("RING: mmap [slot_len=%d][tot_slots=%d] for ring on device %s\n",
	 pfr->slots_info->slot_len, pfr->slots_info->tot_slots,
	 pfr->ring_netdev->name);
#endif

  /* we do not want to have this area swapped out, lock it */
  vma->vm_flags |= VM_LOCKED;
  start = vma->vm_start;

  /* Ring slots start from page 1 (page 0 is reserved for FlowSlotInfo) */
  ptr = (char*)(start+PAGE_SIZE);

  if(remap_page_range(
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
		      vma,
#endif
		      start,
		      __pa(pfr->ring_memory),
		      PAGE_SIZE*pagesToMap, vma->vm_page_prot)) {
#if defined(RING_DEBUG)
    printk("remap_page_range() failed\n");
#endif
    return(-EAGAIN);
  }

#if defined(RING_DEBUG)
  printk("proc_mmap(pagesToMap=%d): success.\n", pagesToMap);
#endif

  return 0;
}

/* ************************************* */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static int ring_recvmsg(struct kiocb *iocb, struct socket *sock,
			struct msghdr *msg, size_t len, int flags)
#else
  static int ring_recvmsg(struct socket *sock, struct msghdr *msg, int len,
			  int flags, struct scm_cookie *scm)
#endif
{
  FlowSlot* slot;
  struct ring_opt *pfr = ring_sk(sock->sk);
  u_int32_t queued_pkts, num_loops = 0;

#if defined(RING_DEBUG)
  printk("ring_recvmsg called\n");
#endif

  slot = get_remove_slot(pfr);

  while((queued_pkts = num_queued_pkts(pfr)) < MIN_QUEUED_PKTS) {
    wait_event_interruptible(pfr->ring_slots_waitqueue, 1);

#if defined(RING_DEBUG)
    printk("-> ring_recvmsg returning %d [queued_pkts=%d][num_loops=%d]\n",
	   slot->slot_state, queued_pkts, num_loops);
#endif

    if(queued_pkts > 0) {
      if(num_loops++ > MAX_QUEUE_LOOPS)
	break;
    }
  }

#if defined(RING_DEBUG)
  if(slot != NULL)
    printk("ring_recvmsg is returning [queued_pkts=%d][num_loops=%d]\n",
	   queued_pkts, num_loops);
#endif

  return(queued_pkts);
}

/* ************************************* */

unsigned int ring_poll(struct file * file,
		       struct socket *sock, poll_table *wait)
{
  FlowSlot* slot;
  struct ring_opt *pfr = ring_sk(sock->sk);

#if defined(RING_DEBUG)
  printk("poll called\n");
#endif

  slot = get_remove_slot(pfr);

  if((slot != NULL) && (slot->slot_state == 0))
    poll_wait(file, &pfr->ring_slots_waitqueue, wait);

#if defined(RING_DEBUG)
  printk("poll returning %d\n", slot->slot_state);
#endif

  if((slot != NULL) && (slot->slot_state == 1))
    return(POLLIN | POLLRDNORM);
  else
    return(0);
}

/* ************************************* */

int add_to_cluster_list(struct ring_cluster *el,
			struct sock *sock) {

  if(el->num_cluster_elements == CLUSTER_LEN)
    return(-1); /* Cluster full */

  ring_sk_datatype(ring_sk(sock))->cluster_id = el->cluster_id;
  el->sk[el->num_cluster_elements] = sock;
  el->num_cluster_elements++;
  return(0);
}

/* ************************************* */

int remove_from_cluster_list(struct ring_cluster *el,
			     struct sock *sock) {
  int i, j;

  for(i=0; i<CLUSTER_LEN; i++)
    if(el->sk[i] == sock) {
      el->num_cluster_elements--;

      if(el->num_cluster_elements > 0) {
	/* The cluster contains other elements */
	for(j=i; j<CLUSTER_LEN-1; j++)
	  el->sk[j] = el->sk[j+1];

	el->sk[CLUSTER_LEN-1] = NULL;
      } else {
	/* Empty cluster */
	memset(el->sk, 0, sizeof(el->sk));
      }

      return(0);
    }

  return(-1); /* Not found */
}

/* ************************************* */

static int remove_from_cluster(struct sock *sock,
			       struct ring_opt *pfr)
{
  struct ring_cluster *el;

#if defined(RING_DEBUG)
  printk("--> remove_from_cluster(%d)\n", pfr->cluster_id);
#endif

  if(pfr->cluster_id == 0 /* 0 = No Cluster */)
    return(0); /* Noting to do */

  el = ring_cluster_list;

  while(el != NULL) {
    if(el->cluster_id == pfr->cluster_id) {
      return(remove_from_cluster_list(el, sock));
    } else
      el = el->next;
  }

  return(-EINVAL); /* Not found */
}

/* ************************************* */

static int add_to_cluster(struct sock *sock,
			  struct ring_opt *pfr,
			  u_short cluster_id)
{
  struct ring_cluster *el;

#ifndef RING_DEBUG
  printk("--> add_to_cluster(%d)\n", cluster_id);
#endif

  if(cluster_id == 0 /* 0 = No Cluster */) return(-EINVAL);

  if(pfr->cluster_id != 0)
    remove_from_cluster(sock, pfr);

  el = ring_cluster_list;

  while(el != NULL) {
    if(el->cluster_id == cluster_id) {
      return(add_to_cluster_list(el, sock));
    } else
      el = el->next;
  }

  /* There's no existing cluster. We need to create one */
  if((el = kmalloc(sizeof(struct ring_cluster), GFP_KERNEL)) == NULL)
    return(-ENOMEM);

  el->cluster_id = cluster_id;
  el->num_cluster_elements = 1;
  el->hashing_mode = cluster_per_flow; /* Default */
  el->hashing_id   = 0;

  memset(el->sk, 0, sizeof(el->sk));
  el->sk[0] = sock;
  el->next = ring_cluster_list;
  ring_cluster_list = el;
  pfr->cluster_id = cluster_id;

  return(0); /* 0 = OK */
}

/* ************************************* */

/* Code taken/inspired from core/sock.c */
static int ring_setsockopt(struct socket *sock,
			   int level, int optname,
			   char *optval, int optlen)
{
  struct ring_opt *pfr = ring_sk(sock->sk);
  int val, found, ret = 0;
  u_int cluster_id;
  char devName[8];

  if((optlen<sizeof(int)) || (pfr == NULL))
    return(-EINVAL);

  if (get_user(val, (int *)optval))
    return -EFAULT;

  found = 1;

  switch(optname)
    {
    case SO_ATTACH_FILTER:
      ret = -EINVAL;
      if (optlen == sizeof(struct sock_fprog)) {
	unsigned int fsize;
	struct sock_fprog fprog;
	struct sk_filter *filter;

	ret = -EFAULT;

	/*
	  NOTE

	  Do not call copy_from_user within a held
	  splinlock (e.g. ring_mgmt_lock) as this caused
	  problems when certain debugging was enabled under
	  2.6.5 -- including hard lockups of the machine.
	*/
	if(copy_from_user(&fprog, optval, sizeof(fprog)))
	  break;

	fsize = sizeof(struct sock_filter) * fprog.len;
	filter = kmalloc(fsize, GFP_KERNEL);

	if(filter == NULL) {
	  ret = -ENOMEM;
	  break;
	}

	if(copy_from_user(filter->insns, fprog.filter, fsize))
	  break;

	filter->len = fprog.len;

	if(sk_chk_filter(filter->insns, filter->len) != 0) {
	  /* Bad filter specified */
	  kfree(filter);
	  pfr->bpfFilter = NULL;
	  break;
	}

	/* get the lock, set the filter, release the lock */
	write_lock(&ring_mgmt_lock);
	pfr->bpfFilter = filter;
	write_unlock(&ring_mgmt_lock);
      }
      ret = 0;
      break;

    case SO_DETACH_FILTER:
      write_lock(&ring_mgmt_lock);
      found = 1;
      if(pfr->bpfFilter != NULL) {
	kfree(pfr->bpfFilter);
	pfr->bpfFilter = NULL;
	write_unlock(&ring_mgmt_lock);
	break;
      }
      ret = -ENONET;
      break;

    case SO_ADD_TO_CLUSTER:
      if (optlen!=sizeof(val))
	return -EINVAL;

      if (copy_from_user(&cluster_id, optval, sizeof(cluster_id)))
	return -EFAULT;

      write_lock(&ring_mgmt_lock);
      ret = add_to_cluster(sock->sk, pfr, cluster_id);
      write_unlock(&ring_mgmt_lock);
      break;

    case SO_REMOVE_FROM_CLUSTER:
      write_lock(&ring_mgmt_lock);
      ret = remove_from_cluster(sock->sk, pfr);
      write_unlock(&ring_mgmt_lock);
      break;

    case SO_SET_REFLECTOR:
      if(optlen >= (sizeof(devName)-1))
	return -EINVAL;

      if(optlen > 0) {
	if(copy_from_user(devName, optval, optlen))
	  return -EFAULT;
      }

      devName[optlen] = '\0';

#if defined(RING_DEBUG)
      printk("+++ SO_SET_REFLECTOR(%s)\n", devName);
#endif

      write_lock(&ring_mgmt_lock);
      pfr->reflector_dev = dev_get_by_name(devName);
      write_unlock(&ring_mgmt_lock);

#if defined(RING_DEBUG)
      if(pfr->reflector_dev != NULL)
	printk("SO_SET_REFLECTOR(%s): succeded\n", devName);
      else
	printk("SO_SET_REFLECTOR(%s): device unknown\n", devName);
#endif
      break;

    default:
      found = 0;
      break;
    }

  if(found)
    return(ret);
  else
    return(sock_setsockopt(sock, level, optname, optval, optlen));
}

/* ************************************* */

static int ring_ioctl(struct socket *sock,
		      unsigned int cmd, unsigned long arg)
{
  switch(cmd)
    {
#ifdef CONFIG_INET
    case SIOCGIFFLAGS:
    case SIOCSIFFLAGS:
    case SIOCGIFCONF:
    case SIOCGIFMETRIC:
    case SIOCSIFMETRIC:
    case SIOCGIFMEM:
    case SIOCSIFMEM:
    case SIOCGIFMTU:
    case SIOCSIFMTU:
    case SIOCSIFLINK:
    case SIOCGIFHWADDR:
    case SIOCSIFHWADDR:
    case SIOCSIFMAP:
    case SIOCGIFMAP:
    case SIOCSIFSLAVE:
    case SIOCGIFSLAVE:
    case SIOCGIFINDEX:
    case SIOCGIFNAME:
    case SIOCGIFCOUNT:
    case SIOCSIFHWBROADCAST:
      return(inet_dgram_ops.ioctl(sock, cmd, arg));
#endif

    default:
      return -ENOIOCTLCMD;
    }

  return 0;
}

/* ************************************* */

static struct proto_ops ring_ops = {
  .family	=	PF_RING,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
  .owner	=	THIS_MODULE,
#endif

  /* Operations that make no sense on ring sockets. */
  .connect	=	sock_no_connect,
  .socketpair	=	sock_no_socketpair,
  .accept	=	sock_no_accept,
  .getname	=	sock_no_getname,
  .listen	=	sock_no_listen,
  .shutdown	=	sock_no_shutdown,
  .sendpage	=	sock_no_sendpage,
  .sendmsg	=	sock_no_sendmsg,
  .getsockopt	=	sock_no_getsockopt,

  /* Now the operations that really occur. */
  .release	=	ring_release,
  .bind		=	ring_bind,
  .mmap		=	ring_mmap,
  .poll		=	ring_poll,
  .setsockopt	=	ring_setsockopt,
  .ioctl	=	ring_ioctl,
  .recvmsg	=	ring_recvmsg,
};

/* ************************************ */

static struct net_proto_family ring_family_ops = {
  .family	=	PF_RING,
  .create	=	ring_create,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
  .owner	=	THIS_MODULE,
#endif
};

// BD: API changed in 2.6.12, ref:
// http://svn.clkao.org/svnweb/linux/revision/?rev=28201
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,11))
static struct proto ring_proto = {
  .name		=	"PF_RING",
  .owner	=	THIS_MODULE,
  .obj_size	=	sizeof(struct sock),
};
#endif

/* ************************************ */

static void __exit ring_exit(void)
{
  struct list_head *ptr;
  struct ring_element *entry;

  for(ptr = ring_table.next; ptr != &ring_table; ptr = ptr->next) {
    entry = list_entry(ptr, struct ring_element, list);
    kfree(entry);
  }

  while(ring_cluster_list != NULL) {
    struct ring_cluster *next = ring_cluster_list->next;
    kfree(ring_cluster_list);
    ring_cluster_list = next;
  }

  set_skb_ring_handler(NULL);
  set_buffer_ring_handler(NULL);
  sock_unregister(PF_RING);
  ring_proc_term();
  printk("PF_RING shut down.\n");
}

/* ************************************ */

static int __init ring_init(void)
{
  printk("Welcome to PF_RING %s\n(C) 2004-06 L.Deri <deri@ntop.org>\n",
	 RING_VERSION);

  INIT_LIST_HEAD(&ring_table);
  ring_cluster_list = NULL;

  sock_register(&ring_family_ops);

  set_skb_ring_handler(skb_ring_handler);
  set_buffer_ring_handler(buffer_ring_handler);

  if(get_buffer_ring_handler() != buffer_ring_handler) {
    printk("PF_RING: set_buffer_ring_handler FAILED\n");

    set_skb_ring_handler(NULL);
    set_buffer_ring_handler(NULL);
    sock_unregister(PF_RING);
    return -1;
  } else {
    printk("PF_RING: bucket length    %d bytes\n", bucket_len);
    printk("PF_RING: ring slots       %d\n", num_slots);
    printk("PF_RING: sample rate      %d [1=no sampling]\n", sample_rate);
    printk("PF_RING: capture TX       %s\n",
	   enable_tx_capture ? "Yes [RX+TX]" : "No [RX only]");
    printk("PF_RING: transparent mode %s\n",
	   transparent_mode ? "Yes" : "No");

    printk("PF_RING initialized correctly.\n");

    ring_proc_init();
    return 0;
  }
}

module_init(ring_init);
module_exit(ring_exit);
MODULE_LICENSE("GPL");

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
MODULE_ALIAS_NETPROTO(PF_RING);
#endif
