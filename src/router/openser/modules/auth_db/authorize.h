/*
 * $Id: authorize.h,v 1.1.1.1 2005/06/13 16:47:33 bogdan_iancu Exp $
 *
 * Digest Authentication - Database support
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


#ifndef AUTHORIZE_H
#define AUTHORIZE_H


#include "../../parser/msg_parser.h"

int auth_db_init(char* db_url);
int auth_db_bind(char* db_url);
void auth_db_close();

/*
 * Authorize using Proxy-Authorization header field
 */
int proxy_authorize(struct sip_msg* _msg, char* _realm, char* _table);


/*
 * Authorize using WWW-Authorization header field
 */
int www_authorize(struct sip_msg* _msg, char* _realm, char* _table);


#endif /* AUTHORIZE_H */
