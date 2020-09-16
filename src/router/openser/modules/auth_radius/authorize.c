/*
 * $Id: authorize.c,v 1.2 2005/06/16 12:41:50 bogdan_iancu Exp $
 *
 * Digest Authentication - Radius support
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
 * -------
 * 2003-03-09: Based on authorize.c from radius_auth (janakj)
 */


#include <string.h>
#include <stdlib.h>
#include "../../mem/mem.h"
#include "../../str.h"
#include "../../parser/hf.h"
#include "../../parser/digest/digest.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../auth/api.h"
#include "authorize.h"
#include "sterman.h"
#include "authrad_mod.h"


/* 
 * Extract URI depending on the request from To or From header 
 */
static inline int get_uri(struct sip_msg* _m, str** _uri)
{
	if ((REQ_LINE(_m).method.len == 8) && (memcmp(REQ_LINE(_m).method.s, "REGISTER", 8) == 0)) {
		if (!_m->to && ((parse_headers(_m, HDR_TO_F, 0) == -1) || !_m->to)) {
			LOG(L_ERR, "get_uri(): To header field not found or malformed\n");
			return -1;
		}
		*_uri = &(get_to(_m)->uri);
	} else {
		if (parse_from_header(_m) == -1) {
			LOG(L_ERR, "get_uri(): Error while parsing headers\n");
			return -2;
		}
		*_uri = &(get_from(_m)->uri);
	}
	return 0;
}


/*
 * Authorize digest credentials
 */
static inline int authorize(struct sip_msg* _msg, str* _realm, int _hftype)
{
	int res;
	auth_result_t ret;
	struct hdr_field* h;
	auth_body_t* cred;
	str* uri;
	struct sip_uri puri;
	str user, domain;

	domain = *_realm;
	ret = auth_api.pre_auth(_msg, &domain, _hftype, &h);
	
	switch(ret) {
	case ERROR:            return 0;
	case NOT_AUTHORIZED:   return -1;
	case DO_AUTHORIZATION: break;
	case AUTHORIZED:       return 1;
	}

	cred = (auth_body_t*)h->parsed;

	if (get_uri(_msg, &uri) < 0) {
		LOG(L_ERR, "authorize(): From/To URI not found\n");
		return -1;
	}
	
	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		LOG(L_ERR, "authorize(): Error while parsing From/To URI\n");
		return -1;
	}

	user.s = (char *)pkg_malloc(puri.user.len);
	if (user.s == NULL) {
		LOG(L_ERR, "authorize: No memory left\n");
		return -1;
	}
	un_escape(&(puri.user), &user);

	res = radius_authorize_sterman(_msg, &cred->digest, 
			&_msg->first_line.u.request.method, &user);
	pkg_free(user.s);

	if (res == 1) {
		ret = auth_api.post_auth(_msg, h);
		switch(ret) {
		case ERROR:          return 0;
		case NOT_AUTHORIZED: return -1;
		case AUTHORIZED:     return 1;
		default:             return -1;
		}
	}

	return -1;
}


/*
 * Authorize using Proxy-Authorize header field
 */
int radius_proxy_authorize(struct sip_msg* _msg, char* _realm, char* _s2)
{
	/* realm parameter is converted to str* in str_fixup */
	return authorize(_msg, (str*)_realm, HDR_PROXYAUTH_T);
}


/*
 * Authorize using WWW-Authorize header field
 */
int radius_www_authorize(struct sip_msg* _msg, char* _realm, char* _s2)
{
	return authorize(_msg, (str*)_realm, HDR_AUTHORIZATION_T);
}

