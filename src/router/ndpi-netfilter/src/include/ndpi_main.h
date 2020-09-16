/*
 * ndpi_main.h
 *
 * Copyright (C) 2011-18 - ntop.org
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

#ifndef __NDPI_MAIN_H__
#define __NDPI_MAIN_H__
#ifdef HAVE_CONFIG_H
#include "ndpi_config.h"
#endif
#include "ndpi_includes.h"
#include "ndpi_define.h"
#include "ndpi_protocol_ids.h"
#include "ndpi_typedefs.h"
#include "ndpi_api.h"
#include "ndpi_protocols.h"

#include "ndpi_kernel_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

NDPI_STATIC  void *ndpi_tdelete(const void * __restrict, void ** __restrict,
		     int (*)(const void *, const void *));
NDPI_STATIC  void *ndpi_tfind(const void *, void *, int (*)(const void *, const void *));
NDPI_STATIC  void *ndpi_tsearch(const void *, void**, int (*)(const void *, const void *));
NDPI_STATIC  void ndpi_twalk(const void *, void (*)(const void *, ndpi_VISIT, int, void*), void *user_data);
NDPI_STATIC  void ndpi_tdestroy(void *vrootp, void (*freefct)(void *));

NDPI_STATIC  int NDPI_BITMASK_COMPARE(NDPI_PROTOCOL_BITMASK a, NDPI_PROTOCOL_BITMASK b);
NDPI_STATIC int NDPI_BITMASK_IS_EMPTY(NDPI_PROTOCOL_BITMASK a);
NDPI_STATIC  void NDPI_DUMP_BITMASK(NDPI_PROTOCOL_BITMASK a);

NDPI_STATIC  u_int8_t ndpi_net_match(u_int32_t ip_to_check,
				 u_int32_t net,
				 u_int32_t num_bits);

NDPI_STATIC u_int8_t ndpi_ips_match(u_int32_t src, u_int32_t dst,
				 u_int32_t net, u_int32_t num_bits);

NDPI_STATIC  u_int16_t ntohs_ndpi_bytestream_to_number(const u_int8_t * str,
					    u_int16_t max_chars_to_read,
					    u_int16_t * bytes_read);

NDPI_STATIC  u_int32_t ndpi_bytestream_to_number(const u_int8_t * str, u_int16_t max_chars_to_read,
				      u_int16_t * bytes_read);
NDPI_STATIC  u_int64_t ndpi_bytestream_to_number64(const u_int8_t * str, u_int16_t max_chars_to_read,
					u_int16_t * bytes_read);
NDPI_STATIC  u_int32_t ndpi_bytestream_dec_or_hex_to_number(const u_int8_t * str,
						 u_int16_t max_chars_to_read,
						 u_int16_t * bytes_read);
NDPI_STATIC  u_int64_t ndpi_bytestream_dec_or_hex_to_number64(const u_int8_t * str,
						   u_int16_t max_chars_to_read,
						   u_int16_t * bytes_read);
NDPI_STATIC  u_int32_t ndpi_bytestream_to_ipv4(const u_int8_t * str, u_int16_t max_chars_to_read,
				    u_int16_t * bytes_read);

NDPI_STATIC  void ndpi_set_detected_protocol(struct ndpi_detection_module_struct *ndpi_struct,
				  struct ndpi_flow_struct *flow,
				  u_int16_t upper_detected_protocol,
				  u_int16_t lower_detected_protocol);

  NDPI_STATIC void ndpi_parse_packet_line_info(struct ndpi_detection_module_struct *ndpi_struct,
					  struct ndpi_flow_struct *flow);
  NDPI_STATIC void ndpi_parse_packet_line_info_any(struct ndpi_detection_module_struct *ndpi_struct,
					      struct ndpi_flow_struct *flow);

  NDPI_STATIC u_int16_t ndpi_check_for_email_address(struct ndpi_detection_module_struct *ndpi_struct,
						struct ndpi_flow_struct *flow, u_int16_t counter);

  NDPI_STATIC void ndpi_int_change_packet_protocol(struct ndpi_detection_module_struct *ndpi_struct,
					      struct ndpi_flow_struct *flow,
					      u_int16_t upper_detected_protocol,
					      u_int16_t lower_detected_protocol);
  NDPI_STATIC void ndpi_int_change_protocol(struct ndpi_detection_module_struct *ndpi_struct,
				       struct ndpi_flow_struct *flow,
				       u_int16_t upper_detected_protocol,
				       u_int16_t lower_detected_protocol);
  NDPI_STATIC void ndpi_int_change_category(struct ndpi_detection_module_struct *ndpi_struct,
				       struct ndpi_flow_struct *flow,
				       ndpi_protocol_category_t protocol_category);

  NDPI_STATIC void ndpi_set_proto_defaults(struct ndpi_detection_module_struct *ndpi_mod,
				      ndpi_protocol_breed_t protoBreed, u_int16_t protoId,
				      u_int8_t can_have_a_subprotocol,
				      u_int16_t tcp_alias_protoId[2],
				      u_int16_t udp_alias_protoId[2], char *protoName,
				      ndpi_protocol_category_t protoCategory,
				      ndpi_port_range *tcpDefPorts,
				      ndpi_port_range *udpDefPorts);

  NDPI_STATIC void ndpi_int_reset_packet_protocol(struct ndpi_packet_struct *packet);
  NDPI_STATIC void ndpi_int_reset_protocol(struct ndpi_flow_struct *flow);

  NDPI_STATIC int ndpi_packet_src_ip_eql(const struct ndpi_packet_struct *packet, const ndpi_ip_addr_t * ip);
  NDPI_STATIC int ndpi_packet_dst_ip_eql(const struct ndpi_packet_struct *packet, const ndpi_ip_addr_t * ip);
  NDPI_STATIC void ndpi_packet_src_ip_get(const struct ndpi_packet_struct *packet, ndpi_ip_addr_t * ip);
  NDPI_STATIC void ndpi_packet_dst_ip_get(const struct ndpi_packet_struct *packet, ndpi_ip_addr_t * ip);

  NDPI_STATIC char *ndpi_get_ip_string(struct ndpi_detection_module_struct *ndpi_struct,
				  const ndpi_ip_addr_t * ip);
  NDPI_STATIC char *ndpi_get_packet_src_ip_string(struct ndpi_detection_module_struct *ndpi_struct,
					     const struct ndpi_packet_struct *packet);
  NDPI_STATIC char* ndpi_get_proto_by_id(struct ndpi_detection_module_struct *ndpi_mod, u_int id);
  NDPI_STATIC u_int16_t ndpi_get_proto_by_name(struct ndpi_detection_module_struct *ndpi_mod, const char *name);

  NDPI_STATIC u_int16_t ndpi_guess_protocol_id(struct ndpi_detection_module_struct *ndpi_struct,
					  struct ndpi_flow_struct *flow,
					  u_int8_t proto, u_int16_t sport, u_int16_t dport,
					  u_int8_t *user_defined_proto);

  NDPI_STATIC u_int8_t ndpi_is_proto(ndpi_protocol p, u_int16_t proto);

  NDPI_STATIC u_int16_t ndpi_get_lower_proto(ndpi_protocol p);
  NDPI_STATIC int ndpi_get_protocol_id_master_proto(struct ndpi_detection_module_struct *ndpi_struct,
					       u_int16_t protocol_id,
					       u_int16_t** tcp_master_proto,
					       u_int16_t** udp_master_proto);
  #/* NDPI_PROTOCOL_NETBIOS */
  NDPI_STATIC int ndpi_netbios_name_interpret(char *in, size_t inlen, char *out, u_int out_len);
  
#ifdef NDPI_ENABLE_DEBUG_MESSAGES
  NDPI_STATIC void ndpi_debug_get_last_log_function_line(struct ndpi_detection_module_struct *ndpi_struct,
					     const char **file, const char **func, u_int32_t * line);
#endif

  /** Checks when the @p payload starts with the string literal @p str.
   * When the string is larger than the payload, check fails.
   * @return non-zero if check succeeded
   */
  NDPI_STATIC int ndpi_match_prefix(const u_int8_t *payload, size_t payload_len,
			const char *str, size_t str_len);

  /* version of ndpi_match_prefix with string literal */
#define ndpi_match_strprefix(payload, payload_len, str)			\
  ndpi_match_prefix((payload), (payload_len), (str), (sizeof(str)-1))

#ifdef __cplusplus
}
#endif

#endif	/* __NDPI_MAIN_H__ */
