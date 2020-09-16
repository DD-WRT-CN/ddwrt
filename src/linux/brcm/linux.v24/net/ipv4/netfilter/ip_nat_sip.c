/*
 * SIP extension for TCP NAT alteration. 
 *
 * Copyright (C) 2004, CyberTAN Corporation
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND CYBERTAN GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. CYBERTAN
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 */
 
#include <linux/module.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ctype.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <linux/netfilter_ipv4/ip_nat.h>
#include <linux/netfilter_ipv4/ip_nat_helper.h>
#include <linux/netfilter_ipv4/ip_nat_rule.h>
#include <linux/netfilter_ipv4/ip_conntrack_sip.h>
#include <linux/netfilter_ipv4/ip_conntrack_helper.h>

#if 0
  #define DEBUGP printk
#else
  #define DEBUGP(format, args...)
#endif

#define MAX_PORTS 8
static int ports[MAX_PORTS];
static int ports_c = 0;

#ifdef MODULE_PARM
MODULE_PARM(ports, "1-" __MODULE_STRING(MAX_PORTS) "i");
#endif

DECLARE_LOCK_EXTERN(ip_sip_lock);

#define RTCP_SUPPORT

/* down(stream): caller -> callee
     up(stream): caller <- callee */


/* Return 1 for match, 0 for accept, -1 for partial. */
static int find_pattern(const char *data, size_t dlen,
			const char *pattern, size_t plen,
			char skip, char term,
			unsigned int *numoff,
			unsigned int *numlen
			)
{
	size_t i, j, k;

//	DEBUGP("find_pattern `%s': dlen = %u\n", pattern, dlen);
	if (dlen == 0)
		return 0;

	if (dlen <= plen) {
		/* Short packet: try for partial? */
		if (strnicmp(data, pattern, dlen) == 0)
			return -1;
		else return 0;
	}

	for(i=0; i<= (dlen - plen); i++){
		if( memcmp(data + i, pattern, plen ) != 0 ) continue;	

		/* patten match !! */	
		*numoff=i + plen;
		for (j=*numoff, k=0; data[j] != term; j++, k++)
			if( j > dlen ) return -1 ;	/* no terminal char */

		*numlen = k;
		return 1;
	}

	return 0;
}

static unsigned int
sip_nat_expected(struct sk_buff **pskb,
		 unsigned int hooknum,
		 struct ip_conntrack *ct,
		 struct ip_nat_info *info)
{
	struct ip_nat_multi_range mr;
	u_int32_t newdstip, newsrcip, newip;
	struct ip_ct_sip_expect *exp_sip_info;
	struct ip_conntrack *master = master_ct(ct);

	IP_NF_ASSERT(info);
	IP_NF_ASSERT(master);
	IP_NF_ASSERT(!(info->initialized & (1<<HOOK2MANIP(hooknum))));

	exp_sip_info = &ct->master->help.exp_sip_info;

	LOCK_BH(&ip_sip_lock);

	/* Outer machine IP */
	newsrcip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip;
	/* Client (virtual) IP under NAT */
	newdstip = master->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip;

	UNLOCK_BH(&ip_sip_lock);

	if (HOOK2MANIP(hooknum) == IP_NAT_MANIP_SRC)
		newip = newsrcip;
	else
		newip = newdstip;

	DEBUGP("sip_nat_expected: IP to %u.%u.%u.%u:%u\n", NIPQUAD(newip),
			htons(exp_sip_info->port));

	mr.rangesize = 1;
	/* We don't want to manip the per-protocol, just the IPs... */
	mr.range[0].flags = IP_NAT_RANGE_MAP_IPS;
	mr.range[0].min_ip = mr.range[0].max_ip = newip;

	/* ... unless we're doing a MANIP_DST, in which case, make
	   sure we map to the correct port */
	if (HOOK2MANIP(hooknum) == IP_NAT_MANIP_DST) {
		mr.range[0].flags |= IP_NAT_RANGE_PROTO_SPECIFIED;
		mr.range[0].min = mr.range[0].max
			= ((union ip_conntrack_manip_proto) 
					{ htons(exp_sip_info->port) });
	}

	return ip_nat_setup_info(ct, &mr, hooknum);
}

static int _find_sip_via_addrport(const char *data, size_t dlen,
			unsigned int *numoff, unsigned int *numlen)
{
	const char *addr, *p = data;
	const char *limit = data + dlen;

	while (p < limit) {
		/* Find the topmost via tag */
		if (strnicmp(p, "\nvia:",5) && strnicmp(p, "\nv:",3) &&
		    strnicmp(p, "\rvia:",5) && strnicmp(p, "\rv:",3)) {
			p++;
			continue;
		}

		/* Look for UDP */
		while (*p!='U' && *p!='u') {
			if (p == limit)
				return 0;
			p++;
		}
		p+= 3;

		if (p >= limit)
			return 0;

		/* Skip a space */
		while (*p == ' '){
			if (p == limit)
				return 0;
			p++;
		}

		/* IP address */
		addr = p;

		/* FQDNs or dotted quads */
		while (isalpha(*p) || isdigit(*p) || (*p=='.') || (*p=='-')) {
			p++;
			if (p == limit)
				return 0;
		}

		/* If there is a port number, skip it */
		if (*p == ':') {
			p++;
			if (p == limit)
				return 0;

			while (isdigit(*p)) {
				p++;
				if (p == limit)
					return 0;
			}
		}

		*numoff = addr - data;
		*numlen = p - addr;

		return 1;
	}

	return 0;	/* Not found */
}

static int _mangle_sip_via(struct ip_conntrack *ct, enum ip_conntrack_info ctinfo,
		struct sk_buff **pskb, u_int32_t newip, u_int16_t newport,
		unsigned int numoff, unsigned int numlen)
{
	int buflen;
	char buffer[sizeof("nnn.nnn.nnn.nnn:nnnnn")];

	sprintf(buffer, "%u.%u.%u.%u:%u", NIPQUAD(newip), newport);
	buflen = strlen(buffer);

	MUST_BE_LOCKED(&ip_sip_lock);

	if (!ip_nat_mangle_udp_packet(pskb, ct, ctinfo, numoff, 
				numlen, buffer, buflen) )
		return 0;

	DEBUGP("(SIP) Via is changed to %s.\n", buffer);

	return buflen - numlen;
}

static int mangle_sip_via(struct ip_conntrack *ct, enum ip_conntrack_info ctinfo,
		struct sk_buff **pskb, u_int32_t newip, u_int16_t newport)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	struct udphdr *udph = (void *)iph + iph->ihl * 4;
	const char *data = (const char *)udph + 8;
	unsigned int udplen = ntohs(iph->tot_len) - (iph->ihl * 4);
	unsigned int datalen = udplen - 8;
	unsigned int matchoff, matchlen;

	 /* Find the topmost via tag */
	_find_sip_via_addrport(data , datalen, &matchoff, &matchlen);
	return _mangle_sip_via(ct, ctinfo, pskb, newip, newport,
			matchoff, matchlen);
}

static int mangle_sip_contact(struct ip_conntrack *ct, enum ip_conntrack_info ctinfo,
		struct sk_buff **pskb, u_int32_t newip, u_int16_t newport)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	struct udphdr *udph = (void *)iph + iph->ihl * 4;
	const char *data = (const char *)udph + 8;
	unsigned int udplen = ntohs(iph->tot_len) - (iph->ihl * 4);
	unsigned int datalen = udplen - 8;

	int buflen, diff_len = 0;
	char buffer[sizeof("nnn.nnn.nnn.nnn:nnnnn")];
	const char *uri, *addr, *p = data;
	const char *limit = data + datalen;


	while (p < limit) {
		if (strnicmp(p, "\ncontact:", 9) && strnicmp(p, "\nm:", 3) &&
		    strnicmp(p, "\rcontact:", 9) && strnicmp(p, "\rm:", 3)) {
			p++;
			continue;
		}

		while (strnicmp(p, "sip:", 4)) {
			if (p == limit)
				return 0;
			p++;
		}
		p += 4;

		/* If there is user info in the contact */
		uri = p;

		while (*p!='@' && *p!='>' && *p!=';' && *p!='\n' && *p!='\r' && *p!='?' && *p!=',') {
			if (p == limit)
				return 0;
			p++;
		}

		if (*p=='@')
			p++;
		else
			p = uri;	/* back to previous URI pointer */

		/* IP address */
		addr = p;

		/* FQDNs or dotted quads */
		while (isalpha(*p) || isdigit(*p) || (*p=='.') || (*p=='-')) {
			p++;
			if (p == limit)
				return 0;
		}

		/* If there is a port number, skip it */
		if (*p==':') {
			p++;
			while (isdigit(*p))
				p++;
		}

		sprintf(buffer, "%u.%u.%u.%u:%u", NIPQUAD(newip), newport);
		buflen = strlen(buffer);

		MUST_BE_LOCKED(&ip_sip_lock);

		if (!ip_nat_mangle_udp_packet(pskb, ct, ctinfo, addr - data, 
					p - addr, buffer, buflen) )
			return 0;
	
		diff_len = buflen - (p - addr);
		DEBUGP("(SIP) Contact is changed to %s.\n", buffer);
		break;
	}

	return diff_len;
}

static int _find_sip_content_length_size(const char *data, size_t dlen,
			unsigned int *numoff, unsigned int *numlen)
{
	char *st, *p = (char *)data;
	const char *limit = data + dlen;
	int size = 0;

	while (p < limit) {
		if (strnicmp(p, "\nContent-Length:", 16) && strnicmp(p, "\nl:", 3) &&
		    strnicmp(p, "\rContent-Length:", 16) && strnicmp(p, "\rm:", 3)) {
			p++;
			continue;
		}

		/* Go through the string above */
		while (*p != ':') {
			if (p == limit)
				return 0;
			p++;
		}
		p++;

		while (*p == ' ') {
			if (p == limit)
				return 0;
			p++;
		}

		st = p;
		size = simple_strtoul(p, &p, 10);

		*numoff = st - data;
		*numlen = p - st;

		return size;
	}

	return 0;
}

static int mangle_sip_content_length(struct ip_conntrack *ct, enum ip_conntrack_info ctinfo,
		struct sk_buff **pskb, int diff_len)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	struct udphdr *udph = (void *)iph + iph->ihl * 4;
	const char *data = (const char *)udph + 8;
	unsigned int udplen = ntohs(iph->tot_len) - (iph->ihl * 4);
	unsigned int datalen = udplen - 8;

	unsigned int matchlen, matchoff;
	int size, buflen;
	char buffer[sizeof("nnnnn")];

	/* original legth */
	size = _find_sip_content_length_size(data, datalen, &matchoff, &matchlen);

	/* new legth */
	sprintf(buffer, "%u", size + diff_len);
	buflen = strlen(buffer);

	if (!ip_nat_mangle_udp_packet(pskb, ct, ctinfo, matchoff, 
				matchlen, buffer, buflen) )
		return 0;

	DEBUGP("(SDP) Content-Length is changed %d->%s.\n", size, buffer);
	return buflen - matchlen;

}

static int mangle_sip_sdp_content(struct ip_conntrack *ct, enum ip_conntrack_info ctinfo,
		struct sk_buff **pskb, u_int32_t newip, u_int16_t newport)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	struct udphdr *udph = (void *)iph + iph->ihl * 4;
	const char *data = (const char *)udph + 8;
	unsigned int udplen = ntohs(iph->tot_len) - (iph->ihl * 4);
	unsigned int datalen = udplen - 8;

	unsigned int matchlen, matchoff;
	int found, buflen, diff_len = 0;
	char buffer[sizeof("nnn.nnn.nnn.nnn")];
	char *addr, *p;
	char *limit = (char *)data + datalen;
	u_int32_t ipaddr = 0;
	u_int16_t getport;
	int dir;

	/* Find RTP address */
	found = find_sdp_rtp_addr(data, datalen, &matchoff, &matchlen, &ipaddr);
	if (found) {
		/* If it's a null address, then the call is on hold */
		if (found == 2 && ipaddr == 0)
			return 0;
			
		sprintf(buffer, "%u.%u.%u.%u", NIPQUAD(newip));
		buflen = strlen(buffer);

		MUST_BE_LOCKED(&ip_sip_lock);

		if (!ip_nat_mangle_udp_packet(pskb, ct, ctinfo, matchoff, 
					matchlen, buffer, buflen) )
			return 0;
	
		diff_len += (buflen - matchlen);
		DEBUGP("(SDP) RTP address is changed to %s.\n", buffer);
	}

	/* Find audio port */
	getport = find_sdp_audio_port(data, datalen, &matchoff, &matchlen);
	if (getport != newport) {
		sprintf(buffer, "%d", newport);
		buflen = strlen(buffer);

		MUST_BE_LOCKED(&ip_sip_lock);

		if (!ip_nat_mangle_udp_packet(pskb, ct, ctinfo, matchoff, 
					matchlen, buffer, buflen) )
			return 0;

		diff_len += (buflen - matchlen);
		DEBUGP("(SDP) audio port is changed to %d.\n", newport);
	}

	dir = CTINFO2DIR(ctinfo);
	if (dir == IP_CT_DIR_ORIGINAL)
		return diff_len;

	/* Find Session ID address */
	found = find_pattern(data, datalen, " IN IP4 ", 8, ' ', '\r',
			&matchoff, &matchlen);
	if (found) {
		p = addr = (char *)data + matchoff;

		/* FQDNs or dotted quads */
		while (isalpha(*p) || isdigit(*p) || (*p=='.') || (*p=='-')) {
			p++;
			if (p == limit)
				return 0;
		}

		sprintf(buffer, "%u.%u.%u.%u", NIPQUAD(newip));
		buflen = strlen(buffer);

		MUST_BE_LOCKED(&ip_sip_lock);

		if (!ip_nat_mangle_udp_packet(pskb, ct, ctinfo, addr - data, 
					p - addr, buffer, buflen) )
			return 0;
	
		diff_len += (buflen - (p - addr));
		DEBUGP("(SDP) Session ID is changed to %s.\n", buffer);
	}

	return diff_len;
}

static int mangle_sip_packet(struct ip_conntrack *ct, enum ip_conntrack_info ctinfo,
		struct sk_buff **pskb, u_int32_t newip)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	struct udphdr *udph = (void *)iph + iph->ihl * 4;
	const char *data = (const char *)udph + 8;
	int diff_len = 0;
	struct ip_ct_sip_master *ct_sip_info = &ct->help.ct_sip_info;
	u_int16_t natport = ntohs(ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u.udp.port); 

	DEBUGP("nat_sip: %s:(%d)\n", __FUNCTION__, __LINE__);

	ct_sip_info->mangled = 1;

	/* Changes the via, if this is a request */
	if (strnicmp(data,"SIP/2.0",7) != 0) {
		mangle_sip_via(ct, ctinfo, pskb, newip, natport);
	}

	mangle_sip_contact(ct, ctinfo, pskb, newip, natport);

	if ((diff_len = mangle_sip_sdp_content(ct, ctinfo, pskb, newip, ct_sip_info->rtpport)) != 0)
		mangle_sip_content_length(ct, ctinfo, pskb, diff_len);

	return 1;
}

static struct ip_conntrack_expect *
expect_find(struct ip_conntrack *ct, u_int32_t dstip, u_int16_t dstport)
{
	const struct ip_conntrack_tuple tuple = { { 0, { 0 } },
			{ dstip, { htons(dstport) }, IPPROTO_UDP }};

	return ip_conntrack_expect_find_get(&tuple);

}

static int sip_out_data_fixup(struct ip_conntrack *ct,
		struct sk_buff **pskb,
		enum ip_conntrack_info ctinfo,
		struct ip_conntrack_expect *expect)
{
	struct ip_conntrack_tuple newtuple;
	struct ip_ct_sip_master *ct_sip_info = &ct->help.ct_sip_info;
	struct ip_ct_sip_expect *exp_sip_info;
	u_int32_t wanip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;     // NAT wan ip
	u_int16_t port = 0;
#ifdef RTCP_SUPPORT
	struct ip_conntrack_expect *rtcp_exp;
#endif

	MUST_BE_LOCKED(&ip_sip_lock);

	if (expect) {
		DEBUGP("nat_sip: %s: There is a exp %p.\n", __FUNCTION__, expect);

		exp_sip_info = &expect->help.exp_sip_info;
		if (exp_sip_info->nated) {
			DEBUGP("nat_sip: %s: The exp %p had been changed.\n", __FUNCTION__, expect);
			goto mangle;
		}

		/* RTP expect */
		if (exp_sip_info->type == CONN_RTP) {
			port = ntohs(expect->tuple.dst.u.udp.port); 
#ifdef RTCP_SUPPORT
			rtcp_exp = expect_find(ct, expect->tuple.dst.ip, port + 1); 
#endif
			/* RFC1889 - If an application is supplied with an odd number
			 * for use as the RTP port, it should replace this number with
			 * the next lower (even) number. */
			if (port % 2) 
				port++;

			/* fullfill newtuple */
			newtuple.dst.ip = wanip;
			newtuple.src.ip = expect->tuple.src.ip;
			newtuple.dst.protonum = expect->tuple.dst.protonum;

			/* Try to get same port: if not, try to change it. */
			for (; port != 0; port += 2) {
				newtuple.dst.u.udp.port = htons(port);
				if (ip_conntrack_change_expect(expect, &newtuple) == 0) {
#ifdef RTCP_SUPPORT
					/* Change RTCP */
					if (rtcp_exp) {
						DEBUGP("nat_sip: %s: RTCP exp %p found.\n",
								__FUNCTION__, rtcp_exp);
						newtuple.dst.u.udp.port = htons(port + 1);
						if (ip_conntrack_change_expect(rtcp_exp, &newtuple) != 0) {
							DEBUGP("nat_sip: %s: Can't change RTCP exp %p.\n",
									__FUNCTION__, rtcp_exp);
							continue;
						}
						rtcp_exp->help.exp_sip_info.nated = 1;
					}
#endif
					break;
				}
			}
			if (port == 0)
				return 0;

			exp_sip_info->nated = 1;
			ct_sip_info->rtpport = port;
			DEBUGP("nat_sip: %s: RTP exp %p, masq port=%d\n", __FUNCTION__, expect, port);
		}
#ifdef RTCP_SUPPORT
		else {
			/* We ignore the RTCP expect, and will adjust it later
			 * during RTP expect */
			DEBUGP("nat_sip: %s: RTCP exp %p, by-pass.\n", __FUNCTION__, expect);
			return 1;
		}
#endif
	}

mangle:
	/* Change address inside packet to match way we're mapping
	   this connection. */
	if (!ct_sip_info->mangled)
		if (!mangle_sip_packet(ct, ctinfo, pskb, wanip))
			return 0;

	return 1;
}

static int sip_in_data_fixup(struct ip_conntrack *ct,
		struct sk_buff **pskb,
		enum ip_conntrack_info ctinfo,
		struct ip_conntrack_expect *expect)
{
	struct iphdr *iph = (*pskb)->nh.iph;
	struct udphdr *udph = (void *)iph + iph->ihl * 4;
	const char *data = (const char *)udph + 8;
	unsigned int udplen = ntohs(iph->tot_len) - (iph->ihl * 4);
	unsigned int datalen = udplen - 8;
	struct ip_ct_sip_master *ct_sip_info = &ct->help.ct_sip_info;
	u_int32_t wanip = ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.ip;     // NAT wan ip
	unsigned int matchlen, matchoff;
	int found, diff_len = 0;
	u_int32_t ipaddr = 0, vip = 0;
	u_int16_t port = 0, vport = 0, aport = 0;
	struct ip_conntrack_expect *exp;
#ifdef RTCP_SUPPORT
	struct ip_conntrack_expect *rtcpexp;
#endif

	if (expect)
		DEBUGP("nat_sip: %s: There is a exp %p.\n", __FUNCTION__, expect);

	/* Prevent from mangling the packet or expect twice */
	if (ct_sip_info->mangled)
		return 1;

	ct_sip_info->mangled = 1;

	if (strnicmp(data, "SIP/2.0 200", 11) == 0) {
		/* Find CSeq field */
		found = find_pattern(data, datalen, "CSeq: ", 6, ' ', '\r',
				&matchoff, &matchlen);
		if (found) {
			char *p = (char *)data + matchoff;

			simple_strtoul(p, &p, 10);
			if (strnicmp(p, " REGISTER", 9) == 0) {
				DEBUGP("nat_sip: 200 OK - REGISTER\n");
				vip = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip;
				vport = ntohs(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.udp.port);
				mangle_sip_via(ct, ctinfo, pskb, vip, vport);
				mangle_sip_contact(ct, ctinfo, pskb, vip, vport);
				return 1;
			}
		}
	}

	/* Only interesting the SDP content. */
	if (strnicmp(data, "INVITE", 6) != 0 && strnicmp(data, "SIP/2.0 200", 11) != 0)
		return 1;
	
	/* Find RTP address */
	found = find_sdp_rtp_addr(data, datalen, &matchoff, &matchlen, &ipaddr);

	DEBUGP("nat_sip: sdp address found = %d, ipaddr = %u.\n", found, ipaddr);
	if (found < 2)
		return 1;

	/* Is it a null address or our WAN address? */
	if (ipaddr == 0 || (htonl(ipaddr) != wanip))
		return 1;

	DEBUGP("nat_sip: %s: This is a lookback RTP connection.\n", __FUNCTION__);

	/* Find audio port, and we don't like the well-known ports,
	 * which is less than 1024 */
	port = find_sdp_audio_port(data, datalen, &matchoff, &matchlen);
	if (port < 1024)
		return 0;
	
	exp = expect_find(ct, wanip, port);
	if (exp) {
		DEBUGP("nat_sip: %s: Found exp %p, tuple.dst=%u.%u.%u.%u:%u.\n",
				__FUNCTION__, exp, NIPQUAD(ipaddr), port);

		/* Restore masq-ed SDP */
		vip = exp->expectant->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.ip;
		aport = exp->help.exp_sip_info.port;
		if ((diff_len = mangle_sip_sdp_content(ct, ctinfo, pskb, vip, aport)) != 0)
			mangle_sip_content_length(ct, ctinfo, pskb, diff_len);

		/* Unset RTP, RTCP expect, respectively */
		ip_conntrack_unexpect_related(exp);
#ifdef RTCP_SUPPORT
		rtcpexp = expect_find(ct, wanip, port + 1);
		if (rtcpexp)
			ip_conntrack_unexpect_related(rtcpexp);
#endif
	}

	return 1;
}

static unsigned int nat_help(struct ip_conntrack *ct,
			 struct ip_conntrack_expect *exp,
			 struct ip_nat_info *info,
			 enum ip_conntrack_info ctinfo,
			 unsigned int hooknum,
			 struct sk_buff **pskb)
{
	int dir;
	struct iphdr *iph = (*pskb)->nh.iph;
	struct udphdr *udph = (void *)iph + iph->ihl * 4;
	const char *data = (const char *)udph + 8;

	/* Only mangle things once: original direction in POST_ROUTING
	   and reply direction on PRE_ROUTING. */
	dir = CTINFO2DIR(ctinfo);
	if (!((hooknum == NF_IP_POST_ROUTING && dir == IP_CT_DIR_ORIGINAL)
	      || (hooknum == NF_IP_PRE_ROUTING && dir == IP_CT_DIR_REPLY))) {
		DEBUGP("nat_sip: Not touching dir %s at hook %s\n",
		       dir == IP_CT_DIR_ORIGINAL ? "ORIG" : "REPLY",
		       hooknum == NF_IP_POST_ROUTING ? "POSTROUTING"
		       : hooknum == NF_IP_PRE_ROUTING ? "PREROUTING"
		       : hooknum == NF_IP_LOCAL_OUT ? "OUTPUT" : "???");
		return NF_ACCEPT;
	}

	if (strnicmp(data, "REGISTER" , 8) == 0)
		DEBUGP("nat_sip: REGISTER\n");
	else if (strnicmp(data, "INVITE" , 6) == 0)
		DEBUGP("nat_sip: INVITE\n");
	else if (strnicmp(data, "ACK" , 3) == 0)
		DEBUGP("nat_sip: ACK\n");
	else if (strnicmp(data, "BYE", 3) == 0)
		DEBUGP("nat_sip: BYE\n");
	else if (strnicmp(data, "SIP/2.0 200", 11) == 0)
		DEBUGP("nat_sip: 200 OK\n");
	else if (strnicmp(data, "SIP/2.0 100", 11) == 0)
		DEBUGP("nat_sip: 100 Trying\n");
	else if (strnicmp(data, "SIP/2.0 180", 11) == 0)
		DEBUGP("nat_sip: 180 Ringing\n");

	LOCK_BH(&ip_sip_lock);

	if (dir == IP_CT_DIR_ORIGINAL)
		sip_out_data_fixup(ct, pskb, ctinfo, exp);
	else
		sip_in_data_fixup(ct, pskb, ctinfo, exp);

	UNLOCK_BH(&ip_sip_lock);

	return NF_ACCEPT;
}

static struct ip_nat_helper sip[MAX_PORTS];
static char sip_names[MAX_PORTS][6];

/* Not __exit: called from init() */
static void fini(void)
{
	int i;

	for (i = 0; (i < MAX_PORTS) && ports[i]; i++) {
		DEBUGP("ip_nat_sip: unregistering port %d\n", ports[i]);
		ip_nat_helper_unregister(&sip[i]);
	}
}

static int __init init(void)
{
	int i, ret=0;
	char *tmpname;

	if (ports[0] == 0)
		ports[0] = SIP_PORT;

	for (i = 0; (i < MAX_PORTS) && ports[i]; i++) {

		memset(&sip[i], 0, sizeof(struct ip_nat_helper));

		sip[i].tuple.dst.u.udp.port = htons(ports[i]);
		sip[i].tuple.dst.protonum = IPPROTO_UDP;
		sip[i].mask.dst.u.udp.port = 0xF0FF;
		sip[i].mask.dst.protonum = 0xFFFF;
		sip[i].help = nat_help;
		sip[i].expect = sip_nat_expected;
		sip[i].flags = IP_NAT_HELPER_F_ALWAYS;

		tmpname = &sip_names[i][0];
		sprintf(tmpname, "sip%2.2d", i);
		sip[i].name = tmpname;

		DEBUGP("ip_nat_sip: Trying to register for port %d\n",
				ports[i]);
		ret = ip_nat_helper_register(&sip[i]);

		if (ret) {
			printk("ip_nat_sip: error registering "
			       "helper for port %d\n", ports[i]);
			fini();
			return ret;
		}
		ports_c++;
	}

	return ret;
}

module_init(init);
module_exit(fini);
