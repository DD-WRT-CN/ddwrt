/*
 * $Id: t_fwd.h,v 1.3 2005/09/02 16:29:17 bogdan_iancu Exp $
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 *
 * History:
 * --------
 *  2003-02-18  added proto to various function prototypes (andrei)
 */


#ifndef _T_FWD_H
#define _T_FWD_H

#include "../../proxy.h"

extern unsigned int gflags_mask;

typedef int (*tfwd_f)(struct sip_msg* p_msg , struct proxy_l * proxy );
typedef int (*taddblind_f)( /*struct cell *t */ );

void e2e_cancel( struct sip_msg *cancel_msg, struct cell *t_cancel,
		struct cell *t_invite );
int e2e_cancel_branch( struct sip_msg *cancel_msg, struct cell *t_cancel,
		struct cell *t_invite, int branch );

int add_uac( struct cell *t, struct sip_msg *request, str *uri,
		str* next_hop, struct proxy_l *proxy, int proto );
int add_blind_uac( );

int t_replicate(struct sip_msg *p_msg, struct proxy_l * proxy,
		int proto);
int t_forward_nonack( struct cell *t, struct sip_msg* p_msg,
		struct proxy_l * p, int proto);
int t_forward_ack( struct sip_msg* p_msg );

void t_on_branch( unsigned int go_to );
unsigned int get_on_branch();

#endif


