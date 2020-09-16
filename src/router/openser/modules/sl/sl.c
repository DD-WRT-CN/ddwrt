/*
 * $Id: sl.c,v 1.2 2005/06/20 14:08:45 bogdan_iancu Exp $
 *
 * sl module
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
 *  2003-03-11  updated to the new module exports interface (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  all mallocs/frees replaced w/ pkg_malloc/pkg_free
 *  2005-03-01  force for stateless replies the incoming interface of
 *              the request (bogdan)
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../script_cb.h"
#include "../../mem/mem.h"
#include "sl_stats.h"
#include "sl_funcs.h"

MODULE_VERSION


static int w_sl_send_reply(struct sip_msg* msg, char* str, char* str2);
static int w_sl_reply_error(struct sip_msg* msg, char* str, char* str2);
static int fixup_sl_send_reply(void** param, int param_no);
static int mod_init(void);
static void mod_destroy();


static cmd_export_t cmds[]={
	{"sl_send_reply",  w_sl_send_reply,  2, fixup_sl_send_reply, REQUEST_ROUTE},
	{"sl_reply_error", w_sl_reply_error, 0, 0,                   REQUEST_ROUTE},
	{0,0,0,0,0}
};


#ifdef STATIC_SL
struct module_exports sl_exports = {
#else
struct module_exports exports= {
#endif
	"sl_module",
	cmds,
	0, /* param exports */
	
	mod_init,   /* module initialization function */
	(response_function) 0,
	mod_destroy,
	0,
	0  /* per-child init function */
};




static int mod_init(void)
{
	fprintf(stderr, "stateless - initializing\n");
	if (init_sl_stats()<0) {
		LOG(L_ERR, "ERROR: init_sl_stats failed\n");
		return -1;
	}
	/* filter all ACKs before script */
	if (register_script_cb(sl_filter_ACK, PRE_SCRIPT_CB|REQ_TYPE_CB, 0 )!=0) {
		LOG(L_ERR,"ERROR:sl:mod_init: register_script_cb failed\n");
		return -1;
	}
	/* init internal SL stuff */
	if (sl_startup()!=0) {
		LOG(L_ERR,"ERROR:sl:mod_init: sl_startup failed\n");
		return -1;
	}

	return 0;
}




static void mod_destroy()
{
	sl_stats_destroy();
	sl_shutdown();
}




static int fixup_sl_send_reply(void** param, int param_no)
{
	unsigned long code;
	int err;

	if (param_no==1){
		code=str2s(*param, strlen(*param), &err);
		if (err==0){
			pkg_free(*param);
			*param=(void*)code;
			return 0;
		}else{
			LOG(L_ERR, "SL module:fixup_sl_send_reply: bad  number <%s>\n",
					(char*)(*param));
			return E_UNSPEC;
		}
	}
	return 0;
}






static int w_sl_send_reply(struct sip_msg* msg, char* str, char* str2)
{
	return sl_send_reply(msg,(unsigned int)(unsigned long)str,str2);
}


static int w_sl_reply_error( struct sip_msg* msg, char* str, char* str2)
{
	return sl_reply_error( msg );
}


