/*
 * Misc useful OS-independent routines.
 *
 * Copyright 2006, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 * $Id: bcmutils.c,v 1.1.1.12 2006/02/27 03:43:16 honor Exp $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <stdarg.h>
#include <bcmutils.h>
#include <osl.h>
#include <sbutils.h>
#include <bcmnvram.h>
#include <bcmendian.h>
#include <bcmdevs.h>
#define NOETHREQ 1
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <proto/bcmip.h>
#include <proto/bcmtcp.h>
#include <proto/802.1d.h>

unsigned char bcm_ctype[] = {
	_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,			/* 0-7 */
	_BCM_C, _BCM_C|_BCM_S, _BCM_C|_BCM_S, _BCM_C|_BCM_S, _BCM_C|_BCM_S, _BCM_C|_BCM_S, _BCM_C,
	_BCM_C,	/* 8-15 */
	_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,			/* 16-23 */
	_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,_BCM_C,			/* 24-31 */
	_BCM_S|_BCM_SP,_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_P,		/* 32-39 */
	_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_P,			/* 40-47 */
	_BCM_D,_BCM_D,_BCM_D,_BCM_D,_BCM_D,_BCM_D,_BCM_D,_BCM_D,			/* 48-55 */
	_BCM_D,_BCM_D,_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_P,			/* 56-63 */
	_BCM_P, _BCM_U|_BCM_X, _BCM_U|_BCM_X, _BCM_U|_BCM_X, _BCM_U|_BCM_X, _BCM_U|_BCM_X,
	_BCM_U|_BCM_X, _BCM_U, /* 64-71 */
	_BCM_U,_BCM_U,_BCM_U,_BCM_U,_BCM_U,_BCM_U,_BCM_U,_BCM_U,			/* 72-79 */
	_BCM_U,_BCM_U,_BCM_U,_BCM_U,_BCM_U,_BCM_U,_BCM_U,_BCM_U,			/* 80-87 */
	_BCM_U,_BCM_U,_BCM_U,_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_P,			/* 88-95 */
	_BCM_P, _BCM_L|_BCM_X, _BCM_L|_BCM_X, _BCM_L|_BCM_X, _BCM_L|_BCM_X, _BCM_L|_BCM_X,
	_BCM_L|_BCM_X, _BCM_L, /* 96-103 */
	_BCM_L,_BCM_L,_BCM_L,_BCM_L,_BCM_L,_BCM_L,_BCM_L,_BCM_L, /* 104-111 */
	_BCM_L,_BCM_L,_BCM_L,_BCM_L,_BCM_L,_BCM_L,_BCM_L,_BCM_L, /* 112-119 */
	_BCM_L,_BCM_L,_BCM_L,_BCM_P,_BCM_P,_BCM_P,_BCM_P,_BCM_C, /* 120-127 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 128-143 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,		/* 144-159 */
	_BCM_S|_BCM_SP, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P,
	_BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P,	/* 160-175 */
	_BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P,
	_BCM_P, _BCM_P, _BCM_P, _BCM_P, _BCM_P,	/* 176-191 */
	_BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U,
	_BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U,	/* 192-207 */
	_BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_P, _BCM_U, _BCM_U, _BCM_U,
	_BCM_U, _BCM_U, _BCM_U, _BCM_U, _BCM_L,	/* 208-223 */
	_BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L,
	_BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L,	/* 224-239 */
	_BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_P, _BCM_L, _BCM_L, _BCM_L,
	_BCM_L, _BCM_L, _BCM_L, _BCM_L, _BCM_L /* 240-255 */
};


ulong
bcm_strtoul(char *cp, char **endp, uint base)
{
	ulong result, value;
	bool minus;

	minus = FALSE;

	while (bcm_isspace(*cp))
		cp++;

	if (cp[0] == '+')
		cp++;
	else if (cp[0] == '-') {
		minus = TRUE;
		cp++;
	}

	if (base == 0) {
		if (cp[0] == '0') {
			if ((cp[1] == 'x') || (cp[1] == 'X')) {
				base = 16;
				cp = &cp[2];
			} else {
				base = 8;
				cp = &cp[1];
			}
		} else
			base = 10;
	} else if (base == 16 && (cp[0] == '0') && ((cp[1] == 'x') || (cp[1] == 'X'))) {
		cp = &cp[2];
	}

	result = 0;

	while (bcm_isxdigit(*cp) &&
	       (value = bcm_isdigit(*cp) ? *cp-'0' : bcm_toupper(*cp)-'A'+10) < base) {
		result = result*base + value;
		cp++;
	}

	if (minus)
		result = (ulong)(result * -1);

	if (endp)
		*endp = (char *)cp;

	return (result);
}

uchar
bcm_toupper(uchar c)
{
	if (bcm_islower(c))
		c -= 'a'-'A';
	return (c);
}

char*
bcm_ether_ntoa(struct ether_addr *ea, char *buf)
{
	sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x",
		ea->octet[0]&0xff, ea->octet[1]&0xff, ea->octet[2]&0xff,
		ea->octet[3]&0xff, ea->octet[4]&0xff, ea->octet[5]&0xff);
	return (buf);
}


/*
 * Search the name=value vars for a specific one and return its value.
 * Returns NULL if not found.
 */
char*
getvar(char *vars, char *name)
{
	char *s;
	int len;

	len = strlen(name);

	/* first look in vars[] */
	for (s = vars; s && *s;) {
		/* CSTYLED */
		if ((memcmp(s, name, len) == 0) && (s[len] == '='))
			return (&s[len+1]);

		while (*s++)
			;
	}

	/* then query nvram */
	return (nvram_get(name));
}

/*
 * Search the vars for a specific one and return its value as
 * an integer. Returns 0 if not found.
 */
int
getintvar(char *vars, char *name)
{
	char *val;

	if ((val = getvar(vars, name)) == NULL)
		return (0);

	return (bcm_strtoul(val, NULL, 0));
}


/*******************************************************************************
 * crc8
 *
 * Computes a crc8 over the input data using the polynomial:
 *
 *       x^8 + x^7 +x^6 + x^4 + x^2 + 1
 *
 * The caller provides the initial value (either CRC8_INIT_VALUE
 * or the previous returned value) to allow for processing of
 * discontiguous blocks of data.  When generating the CRC the
 * caller is responsible for complementing the final return value
 * and inserting it into the byte stream.  When checking, a final
 * return value of CRC8_GOOD_VALUE indicates a valid CRC.
 *
 * Reference: Dallas Semiconductor Application Note 27
 *   Williams, Ross N., "A Painless Guide to CRC Error Detection Algorithms",
 *     ver 3, Aug 1993, ross@guest.adelaide.edu.au, Rocksoft Pty Ltd.,
 *     ftp://ftp.rocksoft.com/clients/rocksoft/papers/crc_v3.txt
 *
 * ****************************************************************************
 */

static uint8 crc8_table[256] = {
    0x00, 0xF7, 0xB9, 0x4E, 0x25, 0xD2, 0x9C, 0x6B,
    0x4A, 0xBD, 0xF3, 0x04, 0x6F, 0x98, 0xD6, 0x21,
    0x94, 0x63, 0x2D, 0xDA, 0xB1, 0x46, 0x08, 0xFF,
    0xDE, 0x29, 0x67, 0x90, 0xFB, 0x0C, 0x42, 0xB5,
    0x7F, 0x88, 0xC6, 0x31, 0x5A, 0xAD, 0xE3, 0x14,
    0x35, 0xC2, 0x8C, 0x7B, 0x10, 0xE7, 0xA9, 0x5E,
    0xEB, 0x1C, 0x52, 0xA5, 0xCE, 0x39, 0x77, 0x80,
    0xA1, 0x56, 0x18, 0xEF, 0x84, 0x73, 0x3D, 0xCA,
    0xFE, 0x09, 0x47, 0xB0, 0xDB, 0x2C, 0x62, 0x95,
    0xB4, 0x43, 0x0D, 0xFA, 0x91, 0x66, 0x28, 0xDF,
    0x6A, 0x9D, 0xD3, 0x24, 0x4F, 0xB8, 0xF6, 0x01,
    0x20, 0xD7, 0x99, 0x6E, 0x05, 0xF2, 0xBC, 0x4B,
    0x81, 0x76, 0x38, 0xCF, 0xA4, 0x53, 0x1D, 0xEA,
    0xCB, 0x3C, 0x72, 0x85, 0xEE, 0x19, 0x57, 0xA0,
    0x15, 0xE2, 0xAC, 0x5B, 0x30, 0xC7, 0x89, 0x7E,
    0x5F, 0xA8, 0xE6, 0x11, 0x7A, 0x8D, 0xC3, 0x34,
    0xAB, 0x5C, 0x12, 0xE5, 0x8E, 0x79, 0x37, 0xC0,
    0xE1, 0x16, 0x58, 0xAF, 0xC4, 0x33, 0x7D, 0x8A,
    0x3F, 0xC8, 0x86, 0x71, 0x1A, 0xED, 0xA3, 0x54,
    0x75, 0x82, 0xCC, 0x3B, 0x50, 0xA7, 0xE9, 0x1E,
    0xD4, 0x23, 0x6D, 0x9A, 0xF1, 0x06, 0x48, 0xBF,
    0x9E, 0x69, 0x27, 0xD0, 0xBB, 0x4C, 0x02, 0xF5,
    0x40, 0xB7, 0xF9, 0x0E, 0x65, 0x92, 0xDC, 0x2B,
    0x0A, 0xFD, 0xB3, 0x44, 0x2F, 0xD8, 0x96, 0x61,
    0x55, 0xA2, 0xEC, 0x1B, 0x70, 0x87, 0xC9, 0x3E,
    0x1F, 0xE8, 0xA6, 0x51, 0x3A, 0xCD, 0x83, 0x74,
    0xC1, 0x36, 0x78, 0x8F, 0xE4, 0x13, 0x5D, 0xAA,
    0x8B, 0x7C, 0x32, 0xC5, 0xAE, 0x59, 0x17, 0xE0,
    0x2A, 0xDD, 0x93, 0x64, 0x0F, 0xF8, 0xB6, 0x41,
    0x60, 0x97, 0xD9, 0x2E, 0x45, 0xB2, 0xFC, 0x0B,
    0xBE, 0x49, 0x07, 0xF0, 0x9B, 0x6C, 0x22, 0xD5,
    0xF4, 0x03, 0x4D, 0xBA, 0xD1, 0x26, 0x68, 0x9F
};

#define CRC_INNER_LOOP(n, c, x) \
	(c) = ((c) >> 8) ^ crc##n##_table[((c) ^ (x)) & 0xff]

uint8
hndcrc8(
	uint8 *pdata,	/* pointer to array of data to process */
	uint  nbytes,	/* number of input data bytes to process */
	uint8 crc	/* either CRC8_INIT_VALUE or previous return value */
)
{
	/* hard code the crc loop instead of using CRC_INNER_LOOP macro
	 * to avoid the undefined and unnecessary (uint8 >> 8) operation.
	 */
	while (nbytes-- > 0)
		crc = crc8_table[(crc ^ *pdata++) & 0xff];

	return crc;
}


/* parse a xx:xx:xx:xx:xx:xx format ethernet address */
int
bcm_ether_atoe(char *p, struct ether_addr *ea)
{
	int i = 0;

	for (;;) {
		ea->octet[i++] = (char) bcm_strtoul(p, &p, 16);
		if (!*p++ || i == 6)
			break;
	}

	return (i == 6);
}

int
bcm_atoi(char *s)
{
	return (int)bcm_strtoul(s, NULL, 10);
}
#undef osl_delay
void
osl_delay(uint usec)
{
	OSL_DELAY(usec);
}
/* Search for token in comma separated token-string */
static int
findmatch(char *string, char *name)
{
	uint len;
	char *c;

	len = strlen(name);
	/* CSTYLED */
	while ((c = strchr(string, ',')) != NULL) {
		if (len == (uint)(c - string) && !strncmp(string, name, len))
			return 1;
		string = c + 1;
	}

	return (!strcmp(string, name));
}

/* Return gpio pin number assigned to the named pin */
/*
* Variable should be in format:
*
*	gpio<N>=pin_name,pin_name
*
* This format allows multiple features to share the gpio with mutual
* understanding.
*
* 'def_pin' is returned if a specific gpio is not defined for the requested functionality
* and if def_pin is not used by others.
*/
uint
getgpiopin(char *vars, char *pin_name, uint def_pin)
{
	char name[] = "gpioXXXX";
	char *val;
	uint pin;

	/* Go thru all possibilities till a match in pin name */
	for (pin = 0; pin < GPIO_NUMPINS; pin ++) {
		sprintf(name, "gpio%d", pin);
		val = getvar(vars, name);
		if (val && findmatch(val, pin_name))
			return pin;
	}

	if (def_pin != GPIO_PIN_NOTDEFINED) {
		/* make sure the default pin is not used by someone else */
		sprintf(name, "gpio%d", def_pin);
		if (getvar(vars, name)) {
			def_pin =  GPIO_PIN_NOTDEFINED;
		}
	}

	return def_pin;
}

/* return pointer to location of substring 'needle' in 'haystack' */
char*
bcmstrstr(char *haystack, char *needle)
{
	int len, nlen;
	int i;

	if ((haystack == NULL) || (needle == NULL))
		return (haystack);

	nlen = strlen(needle);
	len = strlen(haystack) - nlen + 1;

	for (i = 0; i < len; i++)
		if (bcmp(needle, &haystack[i], nlen) == 0)
			return (&haystack[i]);
	return (NULL);
}

/* Takes an Ethernet frame and sets out-of-bound PKTPRIO.
 * Also updates the inplace vlan tag if requested.
 * For debugging, it returns an indication of what it did.
 */
uint
pktsetprio(void *pkt, bool update_vtag)
{
	struct ether_header *eh;
	struct ethervlan_header *evh;
	uint8 *pktdata;
	int priority = 0;
	int rc = 0;

	pktdata = (uint8 *) PKTDATA(NULL, pkt);
	ASSERT(ISALIGNED((uintptr)pktdata, sizeof(uint16)));

	eh = (struct ether_header *) pktdata;

	if (ntoh16(eh->ether_type) == ETHER_TYPE_8021Q) {
		uint16 vlan_tag;
		int vlan_prio, dscp_prio = 0;

		evh = (struct ethervlan_header *)eh;

		vlan_tag = ntoh16(evh->vlan_tag);
		vlan_prio = (int) (vlan_tag >> VLAN_PRI_SHIFT) & VLAN_PRI_MASK;

		if (ntoh16(evh->ether_type) == ETHER_TYPE_IP) {
			uint8 *ip_body = pktdata + sizeof(struct ethervlan_header);
			uint8 tos_tc = IP_TOS(ip_body);
			dscp_prio = (int)(tos_tc >> IPV4_TOS_PREC_SHIFT);
			if ((IP_VER(ip_body) == IP_VER_4) && (IPV4_PROT(ip_body) == IP_PROT_TCP)) {
				int ip_len;
				int src_port;
				bool src_port_exc;
				uint8 *tcp_hdr;

				ip_len = IPV4_PAYLOAD_LEN(ip_body);
				tcp_hdr = IPV4_NO_OPTIONS_PAYLOAD(ip_body);
				src_port = TCP_SRC_PORT(tcp_hdr);
				src_port_exc = (src_port == 10110) || (src_port == 10120) ||
					(src_port == 10130) || (src_port == 10140);

				if ((ip_len == 40) && src_port_exc && TCP_IS_ACK(tcp_hdr)) {
					dscp_prio = 7;
				}
			}
		}

		/* DSCP priority gets precedence over 802.1P (vlan tag) */
		if (dscp_prio != 0) {
			priority = dscp_prio;
			rc |= PKTPRIO_VDSCP;
		} else {
			priority = vlan_prio;
			rc |= PKTPRIO_VLAN;
		}
		/* 
		 * If the DSCP priority is not the same as the VLAN priority,
		 * then overwrite the priority field in the vlan tag, with the
		 * DSCP priority value. This is required for Linux APs because
		 * the VLAN driver on Linux, overwrites the skb->priority field
		 * with the priority value in the vlan tag
		 */
		if (update_vtag && (priority != vlan_prio)) {
			vlan_tag &= ~(VLAN_PRI_MASK << VLAN_PRI_SHIFT);
			vlan_tag |= (uint16)priority << VLAN_PRI_SHIFT;
			evh->vlan_tag = hton16(vlan_tag);
			rc |= PKTPRIO_UPD;
		}
	} else if (ntoh16(eh->ether_type) == ETHER_TYPE_IP) {
		uint8 *ip_body = pktdata + sizeof(struct ether_header);
		uint8 tos_tc = IP_TOS(ip_body);
		priority = (int)(tos_tc >> IPV4_TOS_PREC_SHIFT);
		rc |= PKTPRIO_DSCP;
		if ((IP_VER(ip_body) == IP_VER_4) && (IPV4_PROT(ip_body) == IP_PROT_TCP)) {
			int ip_len;
			int src_port;
			bool src_port_exc;
			uint8 *tcp_hdr;

			ip_len = IPV4_PAYLOAD_LEN(ip_body);
			tcp_hdr = IPV4_NO_OPTIONS_PAYLOAD(ip_body);
			src_port = TCP_SRC_PORT(tcp_hdr);
			src_port_exc = (src_port == 10110) || (src_port == 10120) ||
				(src_port == 10130) || (src_port == 10140);

			if ((ip_len == 40) && src_port_exc && TCP_IS_ACK(tcp_hdr)) {
				priority = 7;
			}
		}
	}

	ASSERT(priority >= 0 && priority <= MAXPRIO);
	PKTSETPRIO(pkt, priority);
	return (rc | priority);
}

void*
osl_malloc(osl_t *osh, uint size)
{
	void *addr;

	/* only ASSERT if osh is defined */
	if (osh)
		ASSERT(osh->magic == OS_HANDLE_MAGIC);

	if ((addr = kmalloc(size, GFP_ATOMIC)) == NULL) {
		if (osh)
			osh->failed++;
		return (NULL);
	}
	if (osh)
		osh->malloced += size;

	return (addr);
}

void
osl_mfree(osl_t *osh, void *addr, uint size)
{
	if (osh) {
		ASSERT(osh->magic == OS_HANDLE_MAGIC);
		osh->malloced -= size;
	}
	kfree(addr);
}

uint
osl_malloced(osl_t *osh)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	return (osh->malloced);
}

uint osl_malloc_failed(osl_t *osh)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	return (osh->failed);
}

