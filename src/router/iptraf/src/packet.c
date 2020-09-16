/* For terms of usage/redistribution/modification see the LICENSE file */
/* For authors and contributors see the AUTHORS file */

/***

packet.c - routines to open the raw socket, read socket data and
           adjust the initial packet pointer

***/

#include "iptraf-ng-compat.h"

#include "deskman.h"
#include "error.h"
#include "options.h"
#include "fltdefs.h"
#include "fltselect.h"
#include "ipfilter.h"
#include "ifaces.h"
#include "packet.h"
#include "ipfrag.h"

#define pkt_cast_hdrp_l2off_t(hdr, pkt, off)			\
	do {							\
		pkt->hdr = (struct hdr *) (pkt->pkt_buf + off);	\
	} while (0)

#define pkt_cast_hdrp_l2(hdr, pkt)				\
	pkt_cast_hdrp_l2off_t(hdr, pkt, 0)


#define pkt_cast_hdrp_l3off_t(hdr, pkt, off)				\
	do {								\
		pkt->hdr = (struct hdr *) (pkt->pkt_payload + off);	\
	} while (0)

#define pkt_cast_hdrp_l3(hdr, pkt)					\
		pkt_cast_hdrp_l3off_t(hdr, pkt, 0)

/* code taken from http://www.faqs.org/rfcs/rfc1071.html. See section 4.1 "C"  */
static int verify_ipv4_hdr_chksum(struct iphdr *ip)
{
	register int sum = 0;
	u_short *addr = (u_short *)ip;
	int len = ip->ihl * 4;

	while (len > 1) {
		sum += *addr++;
		len -= 2;
	}

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ((u_short) ~sum) == 0;
}

static int packet_adjust(struct pkt_hdr *pkt)
{
	int retval = 0;

	switch (pkt->from->sll_hatype) {
	case ARPHRD_ETHER:
	case ARPHRD_LOOPBACK:
		pkt_cast_hdrp_l2(ethhdr, pkt);
		pkt->pkt_payload = pkt->pkt_buf;
		pkt->pkt_payload += ETH_HLEN;
		pkt->pkt_len -= ETH_HLEN;
		break;
	case ARPHRD_SLIP:
	case ARPHRD_CSLIP:
	case ARPHRD_SLIP6:
	case ARPHRD_CSLIP6:
	case ARPHRD_PPP:
	case ARPHRD_TUNNEL:
	case ARPHRD_SIT:
	case ARPHRD_NONE:
	case ARPHRD_IPGRE:
		pkt->pkt_payload = pkt->pkt_buf;
		break;
	case ARPHRD_FRAD:
	case ARPHRD_DLCI:
		pkt->pkt_payload = pkt->pkt_buf;
		pkt->pkt_payload += 4;
		pkt->pkt_len -= 4;
		break;
	case ARPHRD_FDDI:
		pkt_cast_hdrp_l2(fddihdr, pkt);
		pkt->pkt_payload = pkt->pkt_buf;
		pkt->pkt_payload += sizeof(struct fddihdr);
		pkt->pkt_len -= sizeof(struct fddihdr);
		break;
	default:
		/* return a NULL packet to signal an unrecognized link */
		/* protocol to the caller.  Hopefully, this switch statement */
		/* will grow. */
		pkt->pkt_payload = NULL;
		retval = -1;
		break;
	}
	return retval;
}

/* initialize all layer3 protocol pointers (we need to initialize all
 * of them, because of case we change pkt->pkt_protocol) */
static void packet_set_l3_hdrp(struct pkt_hdr *pkt)
{
	switch (pkt->pkt_protocol) {
	case ETH_P_IP:
		pkt_cast_hdrp_l3(iphdr, pkt);
		pkt->ip6_hdr = NULL;
		break;
	case ETH_P_IPV6:
		pkt->iphdr = NULL;
		pkt_cast_hdrp_l3(ip6_hdr, pkt);
		break;
	default:
		pkt->iphdr = NULL;
		pkt->ip6_hdr = NULL;
		break;
	}
}

/* IPTraf input function; reads both keystrokes and network packets. */
int packet_get(int fd, struct pkt_hdr *pkt, int *ch, WINDOW *win)
{
	struct pollfd pfds[2];
	nfds_t nfds = 0;
	int ss;

	/* Monitor raw socket */
	pfds[0].fd = fd;
	pfds[0].events = POLLIN;
	nfds++;

	/* Monitor stdin only if in interactive, not daemon mode. */
	if (ch && !daemonized) {
		pfds[1].fd = 0;
		pfds[1].events = POLLIN;
		nfds++;
	}
	do {
		ss = poll(pfds, nfds, DEFAULT_UPDATE_DELAY);
	} while ((ss == -1) && (errno == EINTR));

	/* no packet ready yet */
	pkt->pkt_len = 0;

	if ((ss > 0) && (pfds[0].revents & POLLIN) != 0) {

		/* these are set upon return from recvmsg() so clean */
		/* them beforehand */
		pkt->msg->msg_controllen = 0;
		pkt->msg->msg_flags = 0;

		ssize_t len = recvmsg(fd, pkt->msg, MSG_TRUNC | MSG_DONTWAIT);
		if (len > 0) {
			pkt->pkt_len = len;
			pkt->pkt_caplen = len;
			if (pkt->pkt_caplen > pkt->pkt_bufsize)
				pkt->pkt_caplen = pkt->pkt_bufsize;
			pkt->pkt_payload = NULL;
			pkt->pkt_protocol = ntohs(pkt->from->sll_protocol);
		} else
			ss = len;
	}

	if (ch) {
		*ch = ERR;	/* signalize we have no key ready */
		if (!daemonized && (ss > 0) && ((pfds[1].revents & POLLIN) != 0))
			*ch = wgetch(win);
	}

	return ss;
}

int packet_process(struct pkt_hdr *pkt, unsigned int *total_br,
		   in_port_t *sport, in_port_t *dport,
		   int match_opposite, int v6inv4asv6)
{
	/* move packet pointer (pkt->pkt_payload) past data link header */
	if (packet_adjust(pkt) != 0)
		return INVALID_PACKET;

again:
	packet_set_l3_hdrp(pkt);
	switch (pkt->pkt_protocol) {
	case ETH_P_IP: {
		struct iphdr *ip = pkt->iphdr;
		in_port_t f_sport = 0, f_dport = 0;

		if (!verify_ipv4_hdr_chksum(ip))
			return CHECKSUM_ERROR;

		if ((ip->protocol == IPPROTO_TCP || ip->protocol == IPPROTO_UDP)
		    && (sport != NULL && dport != NULL)) {
			in_port_t sport_tmp, dport_tmp;

			/*
			 * Process TCP/UDP fragments
			 */
			if (ipv4_is_fragmented(ip)) {
				int firstin = 0;

				/*
				 * total_br contains total byte count of all fragments
				 * not yet retrieved.  Will differ only if fragments
				 * arrived before the first fragment, in which case
				 * the total accumulated fragment sizes will be returned
				 * once the first fragment arrives.
				 */

				if (total_br != NULL)
					*total_br =
					    processfragment(ip, &sport_tmp,
							    &dport_tmp,
							    &firstin);

				if (!firstin)
					return MORE_FRAGMENTS;
			} else {
				struct tcphdr *tcp;
				struct udphdr *udp;
				char *ip_payload = (char *) ip + pkt_iph_len(pkt);

				switch (ip->protocol) {
				case IPPROTO_TCP:
					tcp = (struct tcphdr *) ip_payload;
					sport_tmp = ntohs(tcp->source);
					dport_tmp = ntohs(tcp->dest);
					break;
				case IPPROTO_UDP:
					udp = (struct udphdr *) ip_payload;
					sport_tmp = ntohs(udp->source);
					dport_tmp = ntohs(udp->dest);
					break;
				default:
					sport_tmp = 0;
					dport_tmp = 0;
					break;
				}

				if (total_br != NULL)
					*total_br = pkt->pkt_len;
			}

			if (sport != NULL)
				*sport = sport_tmp;

			if (dport != NULL)
				*dport = dport_tmp;

			f_sport = sport_tmp;
			f_dport = dport_tmp;
		}
		/* Process IP filter */
		if ((ofilter.filtercode != 0)
		    &&
		    (!ipfilter
		     (ip->saddr, ip->daddr, f_sport, f_dport, ip->protocol,
		      match_opposite)))
			return PACKET_FILTERED;
		if (v6inv4asv6 && (ip->protocol == IPPROTO_IPV6)) {
			pkt->pkt_protocol = ETH_P_IPV6;
			pkt->pkt_payload += pkt_iph_len(pkt);
			pkt->pkt_len -= pkt_iph_len(pkt);
			goto again;
		}
		break; }
	case ETH_P_IPV6: {
		struct tcphdr *tcp;
		struct udphdr *udp;
		struct ip6_hdr *ip6 = pkt->ip6_hdr;
		char *ip_payload = (char *) ip6 + pkt_iph_len(pkt);

		//TODO: Filter packets
		switch (pkt_ip_protocol(pkt)) {
		case IPPROTO_TCP:
			tcp = (struct tcphdr *) ip_payload;
			if (sport)
				*sport = ntohs(tcp->source);
			if (dport)
				*dport = ntohs(tcp->dest);
			break;
		case IPPROTO_UDP:
			udp = (struct udphdr *) ip_payload;
			if (sport)
				*sport = ntohs(udp->source);
			if (dport)
				*dport = ntohs(udp->dest);
			break;
		default:
			if (sport)
				*sport = 0;
			if (dport)
				*dport = 0;
			break;
		}
		break; }
	case ETH_P_8021Q:
	case ETH_P_QINQ1:	/* ETH_P_QINQx are not officially */
	case ETH_P_QINQ2:	/* registered IDs */
	case ETH_P_QINQ3:
	case ETH_P_8021AD:
		/* strip 802.1Q/QinQ/802.1ad VLAN header */
		pkt->pkt_payload += 4;
		pkt->pkt_len -= 4;
		/* update network protocol */
		pkt->pkt_protocol = ntohs(*((unsigned short *) pkt->pkt_payload));
		goto again;
	default:
		/* not IPv4 and not IPv6: apply non-IP packet filter */
		if (!nonipfilter(pkt->pkt_protocol)) {
			return PACKET_FILTERED;
		}
	}
	return PACKET_OK;
}

int packet_init(struct pkt_hdr *pkt)
{
	pkt->pkt_buf		= xmallocz(MAX_PACKET_SIZE);
	pkt->pkt_bufsize	= MAX_PACKET_SIZE;
	pkt->pkt_payload	= NULL;
	pkt->ethhdr		= NULL;
	pkt->fddihdr		= NULL;
	pkt->iphdr		= NULL;
	pkt->ip6_hdr		= NULL;
	pkt->pkt_len		= 0;	/* signalize we have no packet prepared */

	pkt->iov.iov_len	= pkt->pkt_bufsize;
	pkt->iov.iov_base	= pkt->pkt_buf;

	pkt->from		= xmallocz(sizeof(*pkt->from));
	pkt->msg		= xmallocz(sizeof(*pkt->msg));

	pkt->msg->msg_name	= pkt->from;
	pkt->msg->msg_namelen	= sizeof(*pkt->from);
	pkt->msg->msg_iov	= &pkt->iov;
	pkt->msg->msg_iovlen	= 1;
	pkt->msg->msg_control	= NULL;

	return 0;	/* all O.K. */
}

void packet_destroy(struct pkt_hdr *pkt)
{
	free(pkt->msg);
	pkt->msg = NULL;

	free(pkt->from);
	pkt->from = NULL;

	free(pkt->pkt_buf);
	pkt->pkt_buf = NULL;

	destroyfraglist();
}

unsigned int packet_get_dropped(int fd)
{
	struct tpacket_stats stats;
	socklen_t len = sizeof(stats);

	memset(&stats, 0, len);
	int err = getsockopt(fd, SOL_PACKET, PACKET_STATISTICS, &stats, &len);
	if (err < 0)
		die_errno("%s(): getsockopt(PACKET_STATISTICS)", __func__);

	return stats.tp_drops;
}

int packet_is_first_fragment(struct pkt_hdr *pkt)
{
	switch (pkt->pkt_protocol) {
	case ETH_P_IP:
		return ipv4_is_first_fragment(pkt->iphdr);
	case ETH_P_IPV6:
		/* FIXME: IPv6 can also be fragmented !!! */
		/* fall through for now */
	default:
		return !0;
	}
}
