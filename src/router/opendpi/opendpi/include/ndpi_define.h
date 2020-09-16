/*
 *
 * Copyright (C) 2011-15 - ntop.org
 * Copyright (C) 2009-2011 by ipoque GmbH
 *
 * This file is part of nDPI, an open source deep packet inspection
 * library based on the OpenDPI and PACE technology by ipoque GmbH
 *
 * nDPI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nDPI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with nDPI.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __NDPI_DEFINE_INCLUDE_FILE__
#define __NDPI_DEFINE_INCLUDE_FILE__

/*
  gcc -E -dM - < /dev/null |grep ENDIAN
*/

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <machine/endian.h>
#endif

#ifdef __OpenBSD__
#include <endian.h>
#define __BYTE_ORDER BYTE_ORDER
#if BYTE_ORDER == LITTLE_ENDIAN
#define __LITTLE_ENDIAN__
#else
#define __BIG_ENDIAN__
#endif				/* BYTE_ORDER */
#endif				/* __OPENBSD__ */


#if 0
#ifndef NDPI_ENABLE_DEBUG_MESSAGES
#define NDPI_ENABLE_DEBUG_MESSAGES
#endif
#endif

#ifdef WIN32
#define __LITTLE_ENDIAN__ 1
#endif

#if !(defined(__LITTLE_ENDIAN__) || defined(__BIG_ENDIAN__))
#if defined(__mips__)
#undef __LITTLE_ENDIAN__
#undef __LITTLE_ENDIAN
#define __BIG_ENDIAN__
#endif

/* Kernel modules */
#if defined(__LITTLE_ENDIAN)
#define __LITTLE_ENDIAN__
#endif
#if defined(__BIG_ENDIAN)
#define __BIG_ENDIAN__
#endif
/* Everything else */
#if (defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__))
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define __LITTLE_ENDIAN__
#else
#define __BIG_ENDIAN__
#endif
#endif

#endif
#if !defined(__bswap16) && !defined(le32toh)
static __inline uint16_t __bswap16(uint16_t __x)
{
	return (__x<<8) | (__x>>8);
}

static __inline uint32_t __bswap32(uint32_t __x)
{
	return (__x>>24) | (__x>>8&0xff00) | (__x<<8&0xff0000) | (__x<<24);
}

static __inline uint64_t __bswap64(uint64_t __x)
{
	return (__bswap32(__x)+(0ULL<<32)) | __bswap32(__x>>32);
}
#endif


#ifdef __LITTLE_ENDIAN__
#define htobe16(x) __bswap16(x)
#define be16toh(x) __bswap16(x)
#define betoh16(x) __bswap16(x)
#define htobe32(x) __bswap32(x)
#define be32toh(x) __bswap32(x)
#define betoh32(x) __bswap32(x)
#define htobe64(x) __bswap64(x)
#define be64toh(x) __bswap64(x)
#define betoh64(x) __bswap64(x)
#define htole16(x) (uint16_t)(x)
#define le16toh(x) (uint16_t)(x)
#define letoh16(x) (uint16_t)(x)
#define htole32(x) (uint32_t)(x)
#define le32toh(x) (uint32_t)(x)
#define letoh32(x) (uint32_t)(x)
#define htole64(x) (uint64_t)(x)
#define le64toh(x) (uint64_t)(x)
#define letoh64(x) (uint64_t)(x)
#else
#define htobe16(x) (uint16_t)(x)
#define be16toh(x) (uint16_t)(x)
#define betoh16(x) (uint16_t)(x)
#define htobe32(x) (uint32_t)(x)
#define be32toh(x) (uint32_t)(x)
#define betoh32(x) (uint32_t)(x)
#define htobe64(x) (uint64_t)(x)
#define be64toh(x) (uint64_t)(x)
#define betoh64(x) (uint64_t)(x)
#define htole16(x) __bswap16(x)
#define le16toh(x) __bswap16(x)
#define letoh16(x) __bswap16(x)
#define htole32(x) __bswap32(x)
#define le32toh(x) __bswap32(x)
#define letoh32(x) __bswap32(x)
#define htole64(x) __bswap64(x)
#define le64toh(x) __bswap64(x)
#define letoh64(x) __bswap64(x)
#endif


#define NDPI_USE_ASYMMETRIC_DETECTION             0
#define NDPI_SELECTION_BITMASK_PROTOCOL_SIZE			u_int32_t

#define NDPI_SELECTION_BITMASK_PROTOCOL_IP			(1<<0)
#define NDPI_SELECTION_BITMASK_PROTOCOL_INT_TCP			(1<<1)
#define NDPI_SELECTION_BITMASK_PROTOCOL_INT_UDP			(1<<2)
#define NDPI_SELECTION_BITMASK_PROTOCOL_INT_TCP_OR_UDP		(1<<3)
#define NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD		(1<<4)
#define NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION	(1<<5)
#define NDPI_SELECTION_BITMASK_PROTOCOL_IPV6			(1<<6)
#define NDPI_SELECTION_BITMASK_PROTOCOL_IPV4_OR_IPV6		(1<<7)
#define NDPI_SELECTION_BITMASK_PROTOCOL_COMPLETE_TRAFFIC	(1<<8)
/* now combined detections */

/* v4 */
#define NDPI_SELECTION_BITMASK_PROTOCOL_TCP (NDPI_SELECTION_BITMASK_PROTOCOL_IP | NDPI_SELECTION_BITMASK_PROTOCOL_INT_TCP)
#define NDPI_SELECTION_BITMASK_PROTOCOL_UDP (NDPI_SELECTION_BITMASK_PROTOCOL_IP | NDPI_SELECTION_BITMASK_PROTOCOL_INT_UDP)
#define NDPI_SELECTION_BITMASK_PROTOCOL_TCP_OR_UDP (NDPI_SELECTION_BITMASK_PROTOCOL_IP | NDPI_SELECTION_BITMASK_PROTOCOL_INT_TCP_OR_UDP)

/* v6 */
#define NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP (NDPI_SELECTION_BITMASK_PROTOCOL_IPV6 | NDPI_SELECTION_BITMASK_PROTOCOL_INT_TCP)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V6_UDP (NDPI_SELECTION_BITMASK_PROTOCOL_IPV6 | NDPI_SELECTION_BITMASK_PROTOCOL_INT_UDP)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP_OR_UDP (NDPI_SELECTION_BITMASK_PROTOCOL_IPV6 | NDPI_SELECTION_BITMASK_PROTOCOL_INT_TCP_OR_UDP)

/* v4 or v6 */
#define NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP (NDPI_SELECTION_BITMASK_PROTOCOL_IPV4_OR_IPV6 | NDPI_SELECTION_BITMASK_PROTOCOL_INT_TCP)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_UDP (NDPI_SELECTION_BITMASK_PROTOCOL_IPV4_OR_IPV6 | NDPI_SELECTION_BITMASK_PROTOCOL_INT_UDP)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_OR_UDP (NDPI_SELECTION_BITMASK_PROTOCOL_IPV4_OR_IPV6 | NDPI_SELECTION_BITMASK_PROTOCOL_INT_TCP_OR_UDP)

#define NDPI_SELECTION_BITMASK_PROTOCOL_TCP_WITH_PAYLOAD		(NDPI_SELECTION_BITMASK_PROTOCOL_TCP | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP_WITH_PAYLOAD		(NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_WITH_PAYLOAD		(NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)

/* does it make sense to talk about udp with payload ??? have you ever seen empty udp packets ? */
#define NDPI_SELECTION_BITMASK_PROTOCOL_UDP_WITH_PAYLOAD		(NDPI_SELECTION_BITMASK_PROTOCOL_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V6_UDP_WITH_PAYLOAD		(NDPI_SELECTION_BITMASK_PROTOCOL_V6_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_UDP_WITH_PAYLOAD		(NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)

#define NDPI_SELECTION_BITMASK_PROTOCOL_TCP_OR_UDP_WITH_PAYLOAD		(NDPI_SELECTION_BITMASK_PROTOCOL_TCP_OR_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP_OR_UDP_WITH_PAYLOAD		(NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP_OR_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_OR_UDP_WITH_PAYLOAD		(NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_OR_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)

#define NDPI_SELECTION_BITMASK_PROTOCOL_TCP_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_TCP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION)

#define NDPI_SELECTION_BITMASK_PROTOCOL_TCP_OR_UDP_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_TCP_OR_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP_OR_UDP_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP_OR_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_OR_UDP_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_OR_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION)

#define NDPI_SELECTION_BITMASK_PROTOCOL_TCP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_TCP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)

#define NDPI_SELECTION_BITMASK_PROTOCOL_TCP_OR_UDP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_TCP_OR_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP_OR_UDP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_V6_TCP_OR_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)
#define NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_OR_UDP_WITH_PAYLOAD_WITHOUT_RETRANSMISSION	(NDPI_SELECTION_BITMASK_PROTOCOL_V4_V6_TCP_OR_UDP | NDPI_SELECTION_BITMASK_PROTOCOL_NO_TCP_RETRANSMISSION | NDPI_SELECTION_BITMASK_PROTOCOL_HAS_PAYLOAD)

/* safe src/dst protocol check macros... */

#define NDPI_SRC_HAS_PROTOCOL(src,protocol) ((src) != NULL && NDPI_COMPARE_PROTOCOL_TO_BITMASK((src)->detected_protocol_bitmask,(protocol)) != 0)

#define NDPI_DST_HAS_PROTOCOL(dst,protocol) ((dst) != NULL && NDPI_COMPARE_PROTOCOL_TO_BITMASK((dst)->detected_protocol_bitmask,(protocol)) != 0)

#define NDPI_SRC_OR_DST_HAS_PROTOCOL(src,dst,protocol) (NDPI_SRC_HAS_PROTOCOL(src,protocol) || NDPI_SRC_HAS_PROTOCOL(dst,protocol))

/**
 * convenience macro to check for excluded protocol
 * a protocol is excluded if the flow is known and either the protocol is not detected at all
 * or the excluded bitmask contains the protocol
 */
#define NDPI_FLOW_PROTOCOL_EXCLUDED(ndpi_struct,flow,protocol) ((flow) != NULL && \
								( NDPI_COMPARE_PROTOCOL_TO_BITMASK((ndpi_struct)->detection_bitmask, (protocol)) == 0 || \
								  NDPI_COMPARE_PROTOCOL_TO_BITMASK((flow)->excluded_protocol_bitmask, (protocol)) != 0 ) )

/* misc definitions */
#define NDPI_DEFAULT_MAX_TCP_RETRANSMISSION_WINDOW_SIZE 0x10000

/* TODO: rebuild all memory areas to have a more aligned memory block here */

/* DEFINITION OF MAX LINE NUMBERS FOR line parse algorithm */
#define NDPI_MAX_PARSE_LINES_PER_PACKET                        64

#define MAX_PACKET_COUNTER                                   65000
#define MAX_DEFAULT_PORTS                                        5

/**********************
 * detection features *
 **********************/
/* #define NDPI_SELECT_DETECTION_WITH_REAL_PROTOCOL ( 1 << 0 ) */

#define NDPI_DIRECTCONNECT_CONNECTION_IP_TICK_TIMEOUT          600
#define NDPI_IRC_CONNECTION_TIMEOUT                            120
#define NDPI_GNUTELLA_CONNECTION_TIMEOUT                       60
#define NDPI_BATTLEFIELD_CONNECTION_TIMEOUT                    60
#define NDPI_THUNDER_CONNECTION_TIMEOUT                        30
#define NDPI_RTSP_CONNECTION_TIMEOUT                           5
#define NDPI_TVANTS_CONNECTION_TIMEOUT                         5
#define NDPI_YAHOO_DETECT_HTTP_CONNECTIONS                     1
#define NDPI_YAHOO_LAN_VIDEO_TIMEOUT                           30
#define NDPI_ZATTOO_CONNECTION_TIMEOUT                         120
#define NDPI_ZATTOO_FLASH_TIMEOUT                              5
#define NDPI_JABBER_STUN_TIMEOUT                               30
#define NDPI_JABBER_FT_TIMEOUT				       5
#define NDPI_SOULSEEK_CONNECTION_IP_TICK_TIMEOUT               600
#define NDPI_MANOLITO_SUBSCRIBER_TIMEOUT                       120
#define NDPI_FTP_CONNECTION_TIMEOUT                            10
#define NDPI_GADGADU_PEER_CONNECTION_TIMEOUT        	       120

#ifdef NDPI_ENABLE_DEBUG_MESSAGES

#define NDPI_LOG(proto, mod, log_level, args...)		\
  {								\
    if(mod != NULL) {						\
      mod->ndpi_debug_print_file=__FILE__;                      \
      mod->ndpi_debug_print_function=__FUNCTION__;              \
      mod->ndpi_debug_print_line=__LINE__;                      \
      mod->ndpi_debug_printf(proto, mod, log_level, args);      \
    }								\
  }

#else				/* NDPI_ENABLE_DEBUG_MESSAGES */

#if defined(WIN32)
#define NDPI_LOG(...) {}
#else
#define NDPI_LOG(proto, mod, log_level, args...) {}
#endif
# define NDPI_LOG(proto, mod, log_level, args...) {}
# define NDPI_LOG_ERR(mod, args...) {}
# define NDPI_LOG_INFO(mod, args...) {}
# define NDPI_LOG_DBG(mod,  args...) {}
# define NDPI_LOG_DBG2(mod, args...) {}

#endif				/* NDPI_ENABLE_DEBUG_MESSAGES */

/**
 * macro for getting the string len of a static string
 *
 * use it instead of strlen to avoid runtime calculations
 */
#define NDPI_STATICSTRING_LEN( s ) ( sizeof( s ) - 1 )

/** macro to compare 2 IPv6 addresses with each other to identify the "smaller" IPv6 address  */
#define NDPI_COMPARE_IPV6_ADDRESS_STRUCTS(x,y)  \
  ((((u_int64_t *)(x))[0]) < (((u_int64_t *)(y))[0]) || ( (((u_int64_t *)(x))[0]) == (((u_int64_t *)(y))[0]) && (((u_int64_t *)(x))[1]) < (((u_int64_t *)(y))[1])) )

//#if !defined(__KERNEL__) && !defined(NDPI_IPTABLES_EXT)
#define NDPI_NUM_BITS              512
//#else
/* custom protocols not supported */
//#define NDPI_NUM_BITS              192
//#endif

#define NDPI_BITS /* 32 */ (sizeof(ndpi_ndpi_mask) * 8 /* number of bits in a byte */)	/* bits per mask */
#define howmanybits(x, y)   (((x)+((y)-1))/(y))

#define NDPI_SET(p, n)    ((p)->fds_bits[(n)/NDPI_BITS] |= (1 << (((u_int32_t)n) % NDPI_BITS)))
#define NDPI_CLR(p, n)    ((p)->fds_bits[(n)/NDPI_BITS] &= ~(1 << (((u_int32_t)n) % NDPI_BITS)))
#define NDPI_ISSET(p, n)  ((p)->fds_bits[(n)/NDPI_BITS] & (1 << (((u_int32_t)n) % NDPI_BITS)))
#define NDPI_ZERO(p)      memset((char *)(p), 0, sizeof(*(p)))
#define NDPI_ONE(p)       memset((char *)(p), 0xFF, sizeof(*(p)))

#define NDPI_NUM_FDS_BITS     howmanybits(NDPI_NUM_BITS, NDPI_BITS)

#define NDPI_PROTOCOL_BITMASK ndpi_protocol_bitmask_struct_t

#define NDPI_BITMASK_ADD(a,b)     NDPI_SET(&a,b)
#define NDPI_BITMASK_DEL(a,b)     NDPI_CLR(&a,b)
#define NDPI_BITMASK_RESET(a)     NDPI_ZERO(&a)
#define NDPI_BITMASK_SET_ALL(a)   NDPI_ONE(&a)
#define NDPI_BITMASK_SET(a, b)    { memcpy(&a, &b, sizeof(NDPI_PROTOCOL_BITMASK)); }

/* this is a very very tricky macro *g*,
  * the compiler will remove all shifts here if the protocol is static...
 */
#define NDPI_ADD_PROTOCOL_TO_BITMASK(bmask,value)     NDPI_SET(&bmask,value)
#define NDPI_DEL_PROTOCOL_FROM_BITMASK(bmask,value)   NDPI_CLR(&bmask,value)
#define NDPI_COMPARE_PROTOCOL_TO_BITMASK(bmask,value) NDPI_ISSET(&bmask,value)

#define NDPI_SAVE_AS_BITMASK(bmask,value)  { NDPI_ZERO(&bmask) ; NDPI_ADD_PROTOCOL_TO_BITMASK(bmask, value); }

#define ndpi_min(a,b)   ((a < b) ? a : b)
#define ndpi_max(a,b)   ((a > b) ? a : b)

#define NDPI_PARSE_PACKET_LINE_INFO(ndpi_struct,flow,packet)		\
                        if (packet->packet_lines_parsed_complete != 1) {        \
			  ndpi_parse_packet_line_info(ndpi_struct,flow);	\
                        }                                                       \

#define NDPI_IPSEC_PROTOCOL_ESP	   50
#define NDPI_IPSEC_PROTOCOL_AH	   51
#define NDPI_GRE_PROTOCOL_TYPE	   0x2F
#define NDPI_ICMP_PROTOCOL_TYPE	   0x01
#define NDPI_IGMP_PROTOCOL_TYPE	   0x02
#define NDPI_EGP_PROTOCOL_TYPE	   0x08
#define NDPI_OSPF_PROTOCOL_TYPE	   0x59
#define NDPI_SCTP_PROTOCOL_TYPE	   132
#define NDPI_IPIP_PROTOCOL_TYPE    0x04
#define NDPI_ICMPV6_PROTOCOL_TYPE  0x3a

#ifdef __KERNEL__
/* the get_uXX will return raw network packet bytes !! */
#include <asm/unaligned.h>
#include <linux/unaligned/packed_struct.h>

#define get_u_int8_t(X,O)  (*(u_int8_t *)(((const u8 *)X) + O))
#define get_u_int16_t(X, O)	__get_unaligned_cpu16(((const u8 *) (X)) + O)
#define get_u_int32_t(X, O)	__get_unaligned_cpu32(((const u8 *) (X)) + O)
#define get_u_int64_t(X, O)	__get_unaligned_cpu64(((const u8 *) (X)) + O)

#define get_l16(X, O)	get_unaligned_le16(((const u8 *) (X)) + O)
#define get_l32(X, O)	get_unaligned_le32(((const u8 *) (X)) + O)

/* new definitions to get little endian from network bytes */
#define get_ul8(X,O) get_u_int8_t(X,O)

#endif

/* define memory callback function */
#define match_first_bytes(payload,st) (memcmp((payload),(st),(sizeof(st)-1))==0)

#define NDPI_MAX_DNS_REQUESTS                   16

/* IMPORTANT: order according to its severity */
#define NDPI_CIPHER_SAFE                        0
#define NDPI_CIPHER_WEAK                        1
#define NDPI_CIPHER_INSECURE                    2

#endif				/* __NDPI_DEFINE_INCLUDE_FILE__ */
