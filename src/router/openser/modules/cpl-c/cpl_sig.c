/*
 * $Id: cpl_sig.c,v 1.4 2005/09/02 10:55:38 bogdan_iancu Exp $
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


#include "../../action.h"
#include "../../dset.h"
#include "../tm/tm_load.h"
#include "loc_set.h"
#include "cpl_sig.h"
#include "cpl_env.h"


/* forwards the msg to the given location set; if flags has set the
 * CPL_PROXY_DONE, all locations will be added as branches, otherwise, the first
 * one will set as RURI (this is ha case when this is the first proxy of the
 * message)
 * The given list of location will be freed, returning 0 instead.
 * Returns:  0 - OK
 *          -1 - error */
int cpl_proxy_to_loc_set( struct sip_msg *msg, struct location **locs,
													unsigned char flag)
{
	struct location *foo;
	struct action act;

	if (!*locs) {
		LOG(L_ERR,"ERROR:cpl_c:cpl_proxy_to_loc_set: empty loc set!!\n");
		goto error;
	}

	/* if it's the first time when this sip_msg is proxied, use the first addr
	 * in loc_set to rewrite the RURI */
	if (!(flag&CPL_PROXY_DONE)) {
		DBG("DEBUG:cpl_c:cpl_proxy_to_loc_set: rewriting Request-URI with "
			"<%s>\n",(*locs)->addr.uri.s);
		/* build a new action for setting the URI */
		act.type = SET_URI_T;
		act.p1_type = STRING_ST;
		act.p1.string = (*locs)->addr.uri.s;
		act.next = 0;
		/* push the action */
		if (do_action(&act, msg) < 0) {
			LOG(L_ERR,"ERROR:cpl_c:cpl_proxy_to_loc_set: do_action failed\n");
			goto error;
		}
		/* is the location NATED? */
		if ((*locs)->flags&CPL_LOC_NATED) setflag(msg,cpl_env.nat_flag);
		/* free the location and point to the next one */
		foo = (*locs)->next;
		free_location( *locs );
		*locs = foo;
	}

	/* add the rest of the locations as branches */
	while(*locs) {
		DBG("DEBUG:cpl_c:cpl_proxy_to_loc_set: appending branch "
			"<%.*s>\n",(*locs)->addr.uri.len,(*locs)->addr.uri.s);
		if(append_branch(msg, &(*locs)->addr.uri, 0, Q_UNSPECIFIED, 0, 0)==-1){
			LOG(L_ERR,"ERROR:cpl_c:cpl_proxy_to_loc_set: failed when "
				"appending branch <%s>\n",(*locs)->addr.uri.s);
			goto error;
		}
		/* is the location NATED? */
		if ((*locs)->flags&CPL_LOC_NATED) setflag(msg,cpl_env.nat_flag);
		/* free the location and point to the next one */
		foo = (*locs)->next;
		free_location( *locs );
		*locs = foo;
	}

	/* run what proxy route is set */
	if (cpl_env.proxy_route) {
		/* do not alter route type - it might be REQUEST or FAILURE */
		if (run_actions( rlist[cpl_env.proxy_route], msg)<0) {
			LOG(L_ERR,"ERROR:cpl_c:cpl_proxy_to_loc_set: "
				"Error in do_action for proxy_route\n");
		}
	}

	/* do t_forward */
	if ( flag&CPL_IS_STATEFUL ) {
		/* transaction exists */
		if (cpl_fct.tmb.t_forward_nonack(msg, 0/*no proxy*/)==-1) {
			LOG(L_ERR,"ERROR:cpl_c:cpl_proxy_to_loc_set: t_forward_nonack "
				"failed !\n");
			goto error;
		}
	} else {
		/* no transaction -> build & fwd */
		if (cpl_fct.tmb.t_relay(msg, 0, 0)==-1) {
			LOG(L_ERR,"ERROR:cpl_c:cpl_proxy_to_loc_set: t_relay failed !\n");
			goto error;
		}
	}

	return 0;
error:
	return -1;
}


