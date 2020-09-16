/*
 * $Id: dset.h,v 1.4 2005/09/02 10:55:37 bogdan_iancu Exp $
 *
 * Copyright (C) 2001-2004 FhG FOKUS
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _DSET_H
#define _DSET_H

#include "ip_addr.h"
#include "qvalue.h"

struct sip_msg;

extern unsigned int nr_branches;


/* 
 * Add a new branch to current transaction 
 */
int append_branch(struct sip_msg* msg, str* uri, str* dst_uri, qvalue_t q,
		int flags, struct socket_info* force_socket);


/*
 * Get the next branch in the current transaction
 */
char* get_branch( int idx, int* len, qvalue_t* q, str* dst_uri, int *flags,
		struct socket_info** force_socket);


/*
 * Empty the array of branches
 */
void clear_branches(void);


/*
 * Create a Contact header field from the
 * list of current branches
 */
char* print_dset(struct sip_msg* msg, int* len);


/* 
 * Set the q value of the Request-URI
 */
void set_ruri_q(qvalue_t q);


/* 
 * Get the q value of the Request-URI
 */
qvalue_t get_ruri_q(void);

int get_request_uri(struct sip_msg* _m, str* _u);
int rewrite_uri(struct sip_msg* _m, str* _s);


#endif /* _DSET_H */
