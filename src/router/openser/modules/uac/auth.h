/*
 * $Id: auth.h,v 1.1 2005/06/16 13:08:34 bogdan_iancu Exp $
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of opnser, a free SIP server.
 *
 * UAC SER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC SER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * History:
 * ---------
 *  2005-01-31  first version (ramona)
 */


#ifndef _UAC_AUTH_H_
#define _UAC_AUTH_H_

#include "../../parser/msg_parser.h"

struct uac_credential {
	str realm;
	str user;
	str passwd;
	struct uac_credential *next;
};


int has_credentials();

int add_credential( unsigned int type, void *val);

void destroy_credentials();

int uac_auth( struct sip_msg *msg);

#endif
