/*
 * $Id: t_fifo.h,v 1.1.1.1 2005/06/13 16:47:46 bogdan_iancu Exp $
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
 *  2003-03-31  200 for INVITE/UAS resent even for UDP (jiri) 
 *  2004-11-15  t_write_xxx can print whatever avp/hdr
 */



#ifndef _TM_T_FIFO_H_
#define _TM_T_FIFO_H_

#include "../../parser/msg_parser.h"
#include "../../sr_module.h"

extern int tm_unix_tx_timeout;

int fixup_t_write( void** param, int param_no);

int parse_tw_append( modparam_t type, void* val);

int init_twrite_lines();

int init_twrite_sock(void);

int t_write_req(struct sip_msg* msg, char* vm_fifo, char* action);

int t_write_unix(struct sip_msg* msg, char* sock_name, char* action);

#endif
