/*
 * Copyright (c) 2004-2006 Maxim Sobolev <sobomax@FreeBSD.org>
 * Copyright (c) 2006-2007 Sippy Software, Inc., http://www.sippysoft.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rtp.h"
#include "rtpp_log.h"
#include "rtpp_cfg_stable.h"
#include "rtpp_defines.h"
#include "rtpp_network.h"
#include "rtpp_record.h"
#include "rtpp_record_private.h"
#include "rtpp_session.h"
#include "rtpp_util.h"

enum record_mode {MODE_LOCAL_PKT, MODE_REMOTE_RTP, MODE_LOCAL_PCAP}; /* MODE_LOCAL_RTP/MODE_REMOTE_PKT? */

struct rtpp_record_channel {
    char spath[PATH_MAX + 1];
    char rpath[PATH_MAX + 1];
    int fd;
    int needspool;
    char rbuf[4096];
    int rbuf_len;
    enum record_mode mode;
};

#define	RRC_CAST(x)	((struct rtpp_record_channel *)(x))

void *
ropen(struct cfg *cf, struct rtpp_session *sp, char *rname, int orig)
{
    struct rtpp_record_channel *rrc;
    const char *sdir, *suffix1, *suffix2;
    char *cp, *tmp;
    int n, port, rval, remote;
    struct sockaddr_storage raddr;
    pcap_hdr_t pcap_hdr;

    remote = (rname != NULL && strncmp("udp:", rname, 4) == 0) ? 1 : 0;

    rrc = malloc(sizeof(*rrc));
    if (rrc == NULL) {
	rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "can't allocate memory");
	return NULL;
    }
    memset(rrc, 0, sizeof(*rrc));

    if (remote) {
	tmp = strdup(rname + 4);
	if (tmp == NULL) {
	    rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "can't allocate memory");
	    return NULL;
	}
	rrc->mode = MODE_REMOTE_RTP;
	rrc->needspool = 0;
	cp = strrchr(tmp, ':');
	if (cp == NULL) {
	    rtpp_log_write(RTPP_LOG_ERR, sp->log, "remote recording target specification should include port number");
	    free(rrc);
	    free(tmp);
	    return NULL;
	}
	*cp = '\0';
	cp++;

	if (sp->rtcp == NULL) {
	    /* Handle RTCP (increase target port by 1) */
	    port = atoi(cp);
	    if (port <= 0 || port > ((sp->rtcp != NULL) ? 65534 : 65535)) {
		rtpp_log_write(RTPP_LOG_ERR, sp->log, "invalid port in the remote recording target specification");
		free(rrc);
		free(tmp);
		return NULL;
	    }
	    sprintf(cp, "%d", port + 1);
	}

	n = resolve(sstosa(&raddr), AF_INET, tmp, cp, AI_PASSIVE);
	if (n != 0) {
	    rtpp_log_write(RTPP_LOG_ERR, sp->log, "ropen: getaddrinfo: %s", gai_strerror(n));
	    free(rrc);
	    free(tmp);
	    return NULL;
	}
	rrc->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (rrc->fd == -1) {
	    rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "ropen: can't create socket");
	    free(rrc);
	    free(tmp);
	    return NULL;
	}
	if (connect(rrc->fd, sstosa(&raddr), SA_LEN(sstosa(&raddr))) == -1) {
	    rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "ropen: can't connect socket");
	    close(rrc->fd);
	    free(rrc);
	    free(tmp);
	    return NULL;
	}
	free(tmp);
	return (void *)(rrc);
    }

    if (cf->stable->rdir == NULL) {
	rtpp_log_write(RTPP_LOG_ERR, sp->log, "directory for saving local recordings is not configured");
	free(rrc);
	return NULL;
    }

    if (cf->stable->record_pcap != 0) {
	rrc->mode = MODE_LOCAL_PCAP;
    } else {
	rrc->mode = MODE_LOCAL_PKT;
    }

    if (sp->record_single_file != 0) {
        suffix1 = suffix2 = "";
    } else {
        suffix1 = (orig != 0) ? ".o" : ".a";
        suffix2 = (sp->rtcp != NULL) ? ".rtp" : ".rtcp";
    }
    if (cf->stable->sdir == NULL) {
	sdir = cf->stable->rdir;
	rrc->needspool = 0;
    } else {
	sdir = cf->stable->sdir;
	rrc->needspool = 1;
	if (rname == NULL) {
	    sprintf(rrc->rpath, "%s/%s=%s%s%s", cf->stable->rdir, sp->call_id, sp->tag_nomedianum,
	      suffix1, suffix2);
	} else {
	    sprintf(rrc->rpath, "%s/%s%s", cf->stable->rdir, rname, suffix2);
	}
    }
    if (rname == NULL) {
	sprintf(rrc->spath, "%s/%s=%s%s%s", sdir, sp->call_id, sp->tag_nomedianum,
	  suffix1, suffix2);
    } else {
	sprintf(rrc->spath, "%s/%s%s", sdir, rname, suffix2);
    }
    rrc->fd = open(rrc->spath, O_WRONLY | O_CREAT | O_TRUNC, DEFFILEMODE);
    if (rrc->fd == -1) {
	rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "can't open file %s for writing",
	  rrc->spath);
	free(rrc);
	return NULL;
    }

    if (rrc->mode == MODE_LOCAL_PCAP) {
	pcap_hdr.magic_number = PCAP_MAGIC;
	pcap_hdr.version_major = PCAP_VER_MAJR;
	pcap_hdr.version_minor = PCAP_VER_MINR;
	pcap_hdr.thiszone = 0;
	pcap_hdr.sigfigs = 0;
	pcap_hdr.snaplen = 65535;
	pcap_hdr.network = PCAP_FORMAT;
	rval = write(rrc->fd, &pcap_hdr, sizeof(pcap_hdr));
	if (rval == -1) {
	    close(rrc->fd);
	    rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "%s: error writing header",
	      rrc->spath);
	    free(rrc);
	    return NULL;
	}
	if (rval < sizeof(pcap_hdr)) {
	    close(rrc->fd);
	    rtpp_log_write(RTPP_LOG_ERR, sp->log, "%s: short write writing header",
	      rrc->spath);
	    free(rrc);
	    return NULL;
	}
    }

    return (void *)(rrc);
}

static int
flush_rbuf(struct rtpp_session *sp, void *rrc)
{
    int rval;

    rval = write(RRC_CAST(rrc)->fd, RRC_CAST(rrc)->rbuf, RRC_CAST(rrc)->rbuf_len);
    if (rval != -1) {
	RRC_CAST(rrc)->rbuf_len = 0;
	return 0;
    }

    rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "error while recording session (%s)",
      (sp->rtcp != NULL) ? "RTP" : "RTCP");
    /* Prevent futher writing if error happens */
    close(RRC_CAST(rrc)->fd);
    RRC_CAST(rrc)->fd = -1;
    return -1;
}

static int
prepare_pkt_hdr_adhoc(struct rtpp_session *sp, struct rtp_packet *packet,
  struct pkt_hdr_adhoc *hdrp, struct sockaddr *daddr, struct sockaddr *ldaddr,
  int ldport, int face)
{

    memset(hdrp, 0, sizeof(*hdrp));
    hdrp->time = packet->rtime;
    if (hdrp->time == -1) {
	rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "can't get current time");
	return -1;
    }
    switch (sstosa(&packet->raddr)->sa_family) {
    case AF_INET:
	hdrp->addr.in4.sin_family = sstosa(&packet->raddr)->sa_family;
	hdrp->addr.in4.sin_port = satosin(&packet->raddr)->sin_port;
	hdrp->addr.in4.sin_addr = satosin(&packet->raddr)->sin_addr;
	break;

    case AF_INET6:
	hdrp->addr.in6.sin_family = sstosa(&packet->raddr)->sa_family;
	hdrp->addr.in6.sin_port = satosin6(&packet->raddr)->sin6_port;
	hdrp->addr.in6.sin_addr = satosin6(&packet->raddr)->sin6_addr;
	break;

    default:
	abort();
    }

    hdrp->plen = packet->size;
    return 0;
}

static uint16_t ip_id = 0;

static int
prepare_pkt_hdr_pcap(struct rtpp_session *sp, struct rtp_packet *packet,
  union pkt_hdr_pcap *hdrp, struct sockaddr *daddr, struct sockaddr *ldaddr,
  int ldport, int face)
{
    struct sockaddr *src_addr, *dst_addr;
    uint16_t src_port, dst_port;
    pcaprec_hdr_t *pcaprec_hdr;
    struct udpip *udpip;
    int pcap_size;
    struct sockaddr_storage tmp_addr;

    if (packet->rtime == -1) {
	rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "can't get current time");
	return -1;
    }

    if (face == 0) {
        src_addr = sstosa(&(packet->raddr));
        src_port = satosin(src_addr)->sin_port;
        dst_addr = packet->laddr;
        dst_port = htons(packet->rport);
    } else {
        src_addr = ldaddr;
        src_port = htons(ldport);
        dst_addr = daddr;
        dst_port = satosin(dst_addr)->sin_port;
    }

    if (sstosa(src_addr)->sa_family != AF_INET) {
	rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "only AF_INET pcap format is supported");
	return -1;
    }

    memset(hdrp, 0, sizeof(*hdrp));

#if (PCAP_FORMAT == DLT_NULL)
    hdrp->null.family = sstosa(src_addr)->sa_family;
    pcaprec_hdr = &(hdrp->null.pcaprec_hdr);
    udpip = &(hdrp->null.udpip);
    pcap_size = sizeof(hdrp->null);
#else
    /* Prepare fake ethernet header */
    hdrp->en10t.ether_type = htons(0x800);
    if (face == 0 && ishostnull(dst_addr) && !ishostnull(src_addr)) {
        if (local4remote(src_addr, &tmp_addr) == 0) {
            dst_addr = sstosa(&tmp_addr);
        }
    }
    memcpy(hdrp->en10t.ether_dhost + 2, &(satosin(dst_addr)->sin_addr), 4);
    if (face != 0 && ishostnull(src_addr) && !ishostnull(dst_addr)) {
        if (local4remote(dst_addr, &tmp_addr) == 0) {
            src_addr = sstosa(&tmp_addr);
        }
    }
    memcpy(hdrp->en10t.ether_shost + 2, &(satosin(src_addr)->sin_addr), 4);
    pcaprec_hdr = &(hdrp->en10t.pcaprec_hdr);
    udpip = &(hdrp->en10t.udpip);
    pcap_size = sizeof(hdrp->en10t);
#endif

    dtime2ts(packet->rtime, &(pcaprec_hdr->ts_sec), &(pcaprec_hdr->ts_usec));
    pcaprec_hdr->orig_len = pcaprec_hdr->incl_len = pcap_size -
      sizeof(*pcaprec_hdr) + packet->size;

    /* Prepare fake IP header */
    udpip->iphdr.ip_v = 4;
    udpip->iphdr.ip_hl = sizeof(udpip->iphdr) >> 2;
    udpip->iphdr.ip_len = htons(sizeof(udpip->iphdr) + sizeof(udpip->udphdr) + packet->size);
    udpip->iphdr.ip_src = satosin(src_addr)->sin_addr;
    udpip->iphdr.ip_dst = satosin(dst_addr)->sin_addr;
    udpip->iphdr.ip_p = IPPROTO_UDP;
    udpip->iphdr.ip_id = htons(ip_id++);
    udpip->iphdr.ip_ttl = 127;
    udpip->iphdr.ip_sum = rtpp_in_cksum(&(udpip->iphdr), sizeof(udpip->iphdr));

    /* Prepare fake UDP header */
    udpip->udphdr.uh_sport = src_port;
    udpip->udphdr.uh_dport = dst_port;
    udpip->udphdr.uh_ulen = htons(sizeof(udpip->udphdr) + packet->size);

    return 0;
}

void
rwrite(struct rtpp_session *sp, void *rrc, struct rtp_packet *packet,
  struct sockaddr *daddr, struct sockaddr *ldaddr, int ldport, int face)
{
    struct iovec v[2];
    union {
	union pkt_hdr_pcap pcap;
	struct pkt_hdr_adhoc adhoc;
    } hdr;
    int rval, hdr_size;
    int (*prepare_pkt_hdr)(struct rtpp_session *, struct rtp_packet *, void *,
      struct sockaddr *, struct sockaddr *, int, int);

    if (RRC_CAST(rrc)->fd == -1)
	return;

    switch (RRC_CAST(rrc)->mode) {
    case MODE_REMOTE_RTP:
	send(RRC_CAST(rrc)->fd, packet->data.buf, packet->size, 0);
	return;

    case MODE_LOCAL_PKT:
	hdr_size = sizeof(hdr.adhoc);
	prepare_pkt_hdr = (void *)&prepare_pkt_hdr_adhoc;
	break;

    case MODE_LOCAL_PCAP:
#if (PCAP_FORMAT == DLT_NULL)
	hdr_size = sizeof(hdr.pcap.null);
#else
        hdr_size = sizeof(hdr.pcap.en10t);
#endif
	prepare_pkt_hdr = (void *)&prepare_pkt_hdr_pcap;
	break;
    }

    /* Check if the write buffer has necessary space, and flush if not */
    if ((RRC_CAST(rrc)->rbuf_len + hdr_size + packet->size > sizeof(RRC_CAST(rrc)->rbuf)) && RRC_CAST(rrc)->rbuf_len > 0)
	if (flush_rbuf(sp, rrc) != 0)
	    return;

    face = (sp->record_single_file == 0) ? 0 : face;

    /* Check if received packet doesn't fit into the buffer, do synchronous write  if so */
    if (RRC_CAST(rrc)->rbuf_len + hdr_size + packet->size > sizeof(RRC_CAST(rrc)->rbuf)) {
	if (prepare_pkt_hdr(sp, packet, (void *)&hdr, daddr, ldaddr, ldport, face) != 0)
	    return;

	v[0].iov_base = (void *)&hdr;
	v[0].iov_len = hdr_size;
	v[1].iov_base = packet->data.buf;
	v[1].iov_len = packet->size;

	rval = writev(RRC_CAST(rrc)->fd, v, 2);
	if (rval != -1)
	    return;

	rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "error while recording session (%s)",
	  (sp->rtcp != NULL) ? "RTP" : "RTCP");
	/* Prevent futher writing if error happens */
	close(RRC_CAST(rrc)->fd);
	RRC_CAST(rrc)->fd = -1;
	return;
    }
    if (prepare_pkt_hdr(sp, packet, (void *)(RRC_CAST(rrc)->rbuf + RRC_CAST(rrc)->rbuf_len),
      daddr, ldaddr, ldport, face) != 0)
	return;
    RRC_CAST(rrc)->rbuf_len += hdr_size;
    memcpy(RRC_CAST(rrc)->rbuf + RRC_CAST(rrc)->rbuf_len, packet->data.buf, packet->size);
    RRC_CAST(rrc)->rbuf_len += packet->size;
}

void
rclose(struct rtpp_session *sp, void *rrc, int keep)
{

    if (RRC_CAST(rrc)->mode != MODE_REMOTE_RTP && RRC_CAST(rrc)->rbuf_len > 0)
	flush_rbuf(sp, rrc);

    if (RRC_CAST(rrc)->fd != -1)
	close(RRC_CAST(rrc)->fd);

    if (RRC_CAST(rrc)->mode == MODE_REMOTE_RTP)
	return;

    if (keep == 0) {
	if (unlink(RRC_CAST(rrc)->spath) == -1)
	    rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "can't remove "
	      "session record %s", RRC_CAST(rrc)->spath);
    } else if (RRC_CAST(rrc)->needspool == 1) {
	if (rename(RRC_CAST(rrc)->spath, RRC_CAST(rrc)->rpath) == -1)
	    rtpp_log_ewrite(RTPP_LOG_ERR, sp->log, "can't move "
	      "session record from spool into permanent storage");
    }

    free(rrc);
}
