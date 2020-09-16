/*
 * $Id: challenge.h,v 1.1.1.1 2005/06/13 16:47:32 bogdan_iancu Exp $
 *
 * Challenge related functions
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
 */


#ifndef CHALLENGE_H
#define CHALLENGE_H

#include "../../parser/msg_parser.h"


/* 
 * Challenge a user agent using WWW-Authenticate header field
 */
int www_challenge(struct sip_msg* _msg, char* _realm, char* _str2);


/*
 * Challenge a user agent using Proxy-Authenticate header field
 */
int proxy_challenge(struct sip_msg* _msg, char* _realm, char* _str2);


/*
 * Remove used credentials from a SIP message header
 */
int consume_credentials(struct sip_msg* _m, char* _s1, char* _s2);


#endif /* AUTH_H */
