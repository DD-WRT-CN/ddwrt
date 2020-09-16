/*
 * $Id: auth_hdr.h,v 1.1 2005/06/16 13:08:34 bogdan_iancu Exp $
 *
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of openser, a free SIP server.
 *
 * UAC OpenSER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * UAC OpenSER-module is distributed in the hope that it will be useful,
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


#ifndef _UAC_AUTH_HDR_H_
#define _UAC_AUTH_HDR_H_

#include "../../str.h"

#include "auth.h"

struct authenticate_body {
	int flags;
	str realm;
	str domain;
	str nonce;
	str opaque;
	str qop;
};

#define AUTHENTICATE_MD5         (1<<0)
#define AUTHENTICATE_MD5SESS     (1<<1)
#define AUTHENTICATE_STALE       (1<<2)
#define QOP_AUTH                 (1<<3)
#define QOP_AUTH_INT             (1<<4)

int parse_authenticate_body( str *body, struct authenticate_body *auth);

str* build_authorization_hdr(int code, str *uri,
		struct uac_credential *crd, struct authenticate_body *auth,
		char *response);

#endif
