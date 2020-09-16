/*
 * $Id: tm.c,v 1.15 2005/10/18 17:03:15 bogdan_iancu Exp $
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
 *  2003-02-18  added t_forward_nonack_{udp, tcp}, t_relay_to_{udp,tcp},
 *               t_replicate_{udp, tcp} (andrei)
 *  2003-02-19  added t_rely_{udp, tcp} (andrei)
 *  2003-03-06  voicemail changes accepted (jiri)
 *  2003-03-10  module export interface updated to the new format (andrei)
 *  2003-03-16  flags export parameter added (janakj)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-03-30  set_kr for requests only (jiri)
 *  2003-04-05  s/reply_route/failure_route, onreply_route introduced (jiri)
 *  2003-04-14  use protocol from uri (jiri)
 *  2003-07-07  added t_relay_to_tls, t_replicate_tls, t_forward_nonack_tls
 *              added #ifdef USE_TCP, USE_TLS
 *              removed t_relay_{udp,tcp,tls} (andrei)
 *  2003-09-26  added t_forward_nonack_uri() - same as t_forward_nonack() but
 *              takes no parameters -> forwards to uri (bogdan)
 *  2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 *  2004-02-18  t_reply exported via FIFO - imported from VM (bogdan)
 *  2004-10-01  added a new param.: restart_fr_on_each_reply (andrei)
 *  2005-05-30  light version of tm_load - find_export dropped -> module 
 *              interface dosen't need to export interncal functions (bogdan)
 */


#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../script_cb.h"
#include "../../fifo_server.h"
#include "../../usr_avp.h"
#include "../../mem/mem.h"
#include "../../unixsock_server.h"

#include "sip_msg.h"
#include "h_table.h"
#include "ut.h"
#include "t_reply.h"
#include "uac_fifo.h"
#include "uac_unixsock.h"
#include "t_fwd.h"
#include "t_lookup.h"
#include "t_stats.h"
#include "callid.h"
#include "t_cancel.h"
#include "t_fifo.h"
#include "tm_load.h"

MODULE_VERSION

/* fixup functions */
static int fixup_t_send_reply(void** param, int param_no);
static int fixup_str2int( void** param, int param_no);
static int fixup_hostport2proxy(void** param, int param_no);
static int fixup_str2regexp(void** param, int param_no);
static int fixup_local_replied(void** param, int param_no);


/* init functions */
static int mod_init(void);
static int child_init(int rank);


/* exported functions */
inline static int w_t_check(struct sip_msg* msg, char* str, char* str2);
inline static int w_t_reply(struct sip_msg* msg, char* str, char* str2);
inline static int w_t_release(struct sip_msg* msg, char* str, char* str2);
inline static int w_t_retransmit_reply(struct sip_msg* p_msg, char* foo,
				char* bar );
inline static int w_t_newtran(struct sip_msg* p_msg, char* foo, char* bar );
inline static int w_t_relay( struct sip_msg  *p_msg , char *_foo, char *_bar);
inline static int w_t_relay_to_udp( struct sip_msg  *p_msg , char *proxy, 
				 char *);
#ifdef USE_TCP
inline static int w_t_relay_to_tcp( struct sip_msg  *p_msg , char *proxy,
				char *);
#endif
#ifdef USE_TLS
inline static int w_t_relay_to_tls( struct sip_msg  *p_msg , char *proxy,
				char *);
#endif
inline static int w_t_replicate( struct sip_msg  *p_msg , 
				char *proxy, /* struct proxy_l *proxy expected */
				char *_foo       /* nothing expected */ );
inline static int w_t_replicate_udp( struct sip_msg  *p_msg , 
				char *proxy, /* struct proxy_l *proxy expected */
				char *_foo       /* nothing expected */ );
#ifdef USE_TCP
inline static int w_t_replicate_tcp( struct sip_msg  *p_msg , 
				char *proxy, /* struct proxy_l *proxy expected */
				char *_foo       /* nothing expected */ );
#endif
#ifdef USE_TLS
inline static int w_t_replicate_tls( struct sip_msg  *p_msg , 
				char *proxy, /* struct proxy_l *proxy expected */
				char *_foo       /* nothing expected */ );
#endif
inline static int w_t_forward_nonack(struct sip_msg* msg, char* str, char* );
inline static int w_t_forward_nonack_uri(struct sip_msg* msg, char* str,char*);
inline static int w_t_forward_nonack_udp(struct sip_msg* msg, char* str,char*);
#ifdef USE_TCP
inline static int w_t_forward_nonack_tcp(struct sip_msg* msg, char* str,char*);
#endif
#ifdef USE_TLS
inline static int w_t_forward_nonack_tls(struct sip_msg* msg, char* str,char*);
#endif
inline static int w_t_on_negative(struct sip_msg* msg, char *go_to, char *foo);
inline static int w_t_on_reply(struct sip_msg* msg, char *go_to, char *foo );
inline static int w_t_on_branch(struct sip_msg* msg, char *go_to, char *foo );
inline static int t_check_status(struct sip_msg* msg, char *regexp, char *foo);
inline static int t_flush_flags(struct sip_msg* msg, char *foo, char *bar);
inline static int t_local_replied(struct sip_msg* msg, char *type, char *bar);
inline static int t_check_trans(struct sip_msg* msg, char *foo, char *bar);
inline static int t_was_cancelled(struct sip_msg* msg, char *foo, char *bar);


/* strings with avp definition */
static char *fr_timer_param = FR_TIMER_AVP;
static char *fr_inv_timer_param = FR_INV_TIMER_AVP;

static char *bf_mask_param = NULL;


static cmd_export_t cmds[]={
	{"t_newtran",            w_t_newtran,             0, 0,
			REQUEST_ROUTE},
	{"t_lookup_request",     w_t_check,               0, 0,
			REQUEST_ROUTE},
	{"t_reply",              w_t_reply,               2, fixup_t_send_reply,
			REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_retransmit_reply",   w_t_retransmit_reply,    0, 0,
			REQUEST_ROUTE},
	{"t_release",            w_t_release,             0, 0,
			REQUEST_ROUTE},
	{"t_relay_to_udp",       w_t_relay_to_udp,        2, fixup_hostport2proxy,
			REQUEST_ROUTE|FAILURE_ROUTE},
#ifdef USE_TCP
	{"t_relay_to_tcp",       w_t_relay_to_tcp,        2, fixup_hostport2proxy,
			REQUEST_ROUTE|FAILURE_ROUTE},
#endif
#ifdef USE_TLS
	{"t_relay_to_tls",       w_t_relay_to_tls,        2, fixup_hostport2proxy,
			REQUEST_ROUTE|FAILURE_ROUTE},
#endif
	{"t_replicate",          w_t_replicate,           2, fixup_hostport2proxy,
			REQUEST_ROUTE},
	{"t_replicate_udp",      w_t_replicate_udp,       2, fixup_hostport2proxy,
			REQUEST_ROUTE},
#ifdef USE_TCP
	{"t_replicate_tcp",      w_t_replicate_tcp,       2, fixup_hostport2proxy,
			REQUEST_ROUTE},
#endif
#ifdef USE_TLS
	{"t_replicate_tls",      w_t_replicate_tls,       2, fixup_hostport2proxy,
			REQUEST_ROUTE},
#endif
	{"t_relay",              w_t_relay,               0, 0,
			REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_forward_nonack",     w_t_forward_nonack,      2, fixup_hostport2proxy,
			REQUEST_ROUTE},
	{"t_forward_nonack_uri", w_t_forward_nonack_uri,  0, 0,
			REQUEST_ROUTE},
	{"t_forward_nonack_udp", w_t_forward_nonack_udp,  2, fixup_hostport2proxy,
			REQUEST_ROUTE},
#ifdef USE_TCP
	{"t_forward_nonack_tcp", w_t_forward_nonack_tcp,  2, fixup_hostport2proxy,
			REQUEST_ROUTE},
#endif
#ifdef USE_TLS
	{"t_forward_nonack_tls", w_t_forward_nonack_tls,  2, fixup_hostport2proxy,
			REQUEST_ROUTE},
#endif
	{"t_on_failure",         w_t_on_negative,         1, fixup_str2int,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
	{"t_on_reply",           w_t_on_reply,            1, fixup_str2int,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
	{"t_on_branch",          w_t_on_branch,           1, fixup_str2int,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
	{"t_check_status",       t_check_status,          1, fixup_str2regexp,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE },
	{"t_write_req",         t_write_req,              2, fixup_t_write,
			REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_write_unix",        t_write_unix,             2, fixup_t_write,
			REQUEST_ROUTE | FAILURE_ROUTE },
	{"t_flush_flags",       t_flush_flags,            0, 0,
			REQUEST_ROUTE },
	{"t_local_replied",     t_local_replied,          1, fixup_local_replied,
			REQUEST_ROUTE | FAILURE_ROUTE | ONREPLY_ROUTE  },
	{"t_check_trans",       t_check_trans,            0, 0,
			REQUEST_ROUTE },
	{"t_was_cancelled",     t_was_cancelled,          0, 0,
			FAILURE_ROUTE | ONREPLY_ROUTE },
	{"load_tm",             (cmd_function)load_tm,    0, 0,
			0},
	{0,0,0,0,0}
};


static param_export_t params[]={
	{"ruri_matching",             INT_PARAM,
		&ruri_matching},
	{"via1_matching",             INT_PARAM,
		&via1_matching},
	{"fr_timer",                  INT_PARAM,
		&(timer_id2timeout[FR_TIMER_LIST])},
	{"fr_inv_timer",              INT_PARAM,
		&(timer_id2timeout[FR_INV_TIMER_LIST])},
	{"wt_timer",                  INT_PARAM,
		&(timer_id2timeout[WT_TIMER_LIST])},
	{"delete_timer",              INT_PARAM,
		&(timer_id2timeout[DELETE_LIST])},
	{"retr_timer1p1",             INT_PARAM,
		&(timer_id2timeout[RT_T1_TO_1])},
	{"retr_timer1p2",             INT_PARAM,
		&(timer_id2timeout[RT_T1_TO_2])},
	{"retr_timer1p3",             INT_PARAM,
		&(timer_id2timeout[RT_T1_TO_3])},
	{"retr_timer2",               INT_PARAM,
		&(timer_id2timeout[RT_T2])},
	{"noisy_ctimer",              INT_PARAM,
		&noisy_ctimer},
	{"unix_tx_timeout",           INT_PARAM,
		&tm_unix_tx_timeout},
	{"restart_fr_on_each_reply",  INT_PARAM,
		&restart_fr_on_each_reply},
	{"fr_timer_avp",              STR_PARAM,
		&fr_timer_param},
	{"fr_inv_timer_avp",          STR_PARAM,
		&fr_inv_timer_param},
	{"tw_append",                 STR_PARAM|USE_FUNC_PARAM,
		(void*)parse_tw_append },
	{"branch_flag_mask",          STR_PARAM,
		&bf_mask_param },
	{0,0,0}
};


#ifdef STATIC_TM
struct module_exports tm_exports = {
#else
struct module_exports exports= {
#endif
	"tm",
	/* -------- exported functions ----------- */
	cmds,
	/* ------------ exported variables ---------- */
	params,
	
	mod_init, /* module initialization function */
	(response_function) reply_received,
	(destroy_function) tm_shutdown,
	0, /* w_onbreak, */
	child_init /* per-child init function */
};



/**************************** fixup functions ******************************/
static int fixup_str2int( void** param, int param_no)
{
	unsigned long go_to;
	int err;

	if (param_no==1) {
		go_to=str2s(*param, strlen(*param), &err );
		if (err==0) {
			pkg_free(*param);
			*param=(void *)go_to;
			return 0;
		} else {
			LOG(L_ERR, "ERROR: fixup_str2int: bad number <%s>\n",
				(char *)(*param));
			return E_CFG;
		}
	}
	return 0;
}


/* (char *hostname, char *port_nr) ==> (struct proxy_l *, -)  */
static int fixup_hostport2proxy(void** param, int param_no)
{
	unsigned int port;
	char *host;
	int err;
	struct proxy_l *proxy;
	str s;
	
	DBG("TM module: fixup_hostport2proxy(%s, %d)\n", (char*)*param, param_no);
	if (param_no==1){
		DBG("TM module: fixup_hostport2proxy: param 1.. do nothing, wait for #2\n");
		return 0;
	} else if (param_no==2) {

		host=(char *) (*(param-1)); 
		port=str2s(*param, strlen(*param), &err);
		if (err!=0) {
			LOG(L_ERR, "TM module:fixup_hostport2proxy: bad port number <%s>\n",
				(char*)(*param));
			 return E_UNSPEC;
		}
		s.s = host;
		s.len = strlen(host);
		proxy=mk_proxy(&s, port, 0); /* FIXME: udp or tcp? */
		if (proxy==0) {
			LOG(L_ERR, "ERROR: fixup_hostport2proxy: bad host name in URI <%s>\n",
				host );
			return E_BAD_ADDRESS;
		}
		/* success -- fix the first parameter to proxy now ! */

		/* FIXME: janakj, mk_proxy doesn't make copy of host !! */
		/*pkg_free( *(param-1)); you're right --andrei*/
		*(param-1)=proxy;
		return 0;
	} else {
		LOG(L_ERR,"ERROR: fixup_hostport2proxy called with parameter #<>{1,2}\n");
		return E_BUG;
	}
}


/* (char *code, char *reason_phrase)==>(int code, r_p as is) */
static int fixup_t_send_reply(void** param, int param_no)
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
			LOG(L_ERR, "TM module:fixup_t_send_reply: bad  number <%s>\n",
					(char*)(*param));
			return E_UNSPEC;
		}
	}
	/* second param => no conversion*/
	return 0;
}


static int fixup_str2regexp(void** param, int param_no)
{
	regex_t* re;

	if (param_no==1) {
		if ((re=pkg_malloc(sizeof(regex_t)))==0)
			return E_OUT_OF_MEM;
		if (regcomp(re, *param, REG_EXTENDED|REG_ICASE|REG_NEWLINE) ) {
			pkg_free(re);
			LOG(L_ERR,"ERROR: %s : bad re %s\n", exports.name, (char*)*param);
			return E_BAD_RE;
		}
		/* free string */
		pkg_free(*param);
		/* replace it with the compiled re */
		*param=re;
	} else {
		LOG(L_ERR,"ERROR: fixup_str2regexp called with parameter != 1\n");
		return E_BUG;
	}
	return 0;
}


static int fixup_local_replied(void** param, int param_no)
{
	char *val;
	int n = 0;

	if (param_no==1) {
		val = (char*)*param;
		if (strcasecmp(val,"all")==0) {
			n = 1;
		} else if (strcasecmp(val,"last")==0) {
			n = 0;
		} else {
			LOG(L_ERR,"ERROR:tm:fixup_local_replied: invalid param \"%s\"\n",
				val);
			return E_UNSPEC;
		}
		/* free string */
		pkg_free(*param);
		/* replace it with the compiled re */
		*param=(void*)(long)n;
	} else {
		LOG(L_ERR,"ERROR:fixup_local_replied: called with parameter != 1\n");
		return E_BUG;
	}
	return 0;
}



/***************************** init functions *****************************/
int load_tm( struct tm_binds *tmb)
{
	tmb->register_tmcb = register_tmcb;

	/* replicate function */
	tmb->t_replicate = w_t_replicate;
	tmb->t_replicate_udp = w_t_replicate_udp;
#ifdef USE_TCP
	tmb->t_replicate_tcp = w_t_replicate_tcp;
#endif
#ifdef USE_TLS
	tmb->t_replicate_tls = w_t_replicate_tls;
#endif
	/* relay / reply function */
#ifdef USE_TCP
	tmb->t_relay_to_tcp = w_t_relay_to_tcp;
#endif
#ifdef USE_TLS
	tmb->t_relay_to_tls = w_t_relay_to_tls;
#endif
	tmb->t_relay_to_udp = w_t_relay_to_udp;
	tmb->t_relay = w_t_relay;
	tmb->t_forward_nonack = (tfwd_f)w_t_forward_nonack;
	tmb->t_reply = (treply_f)w_t_reply;
	tmb->t_reply_with_body = t_reply_with_body;

	/* transaction location/status functions */
	tmb->t_newtran = t_newtran;
	tmb->t_is_local = t_is_local;
	tmb->t_get_trans_ident = t_get_trans_ident;
	tmb->t_lookup_ident = t_lookup_ident;
	tmb->t_gett = get_t;

	/* tm uac functions */
	tmb->t_addblind = add_blind_uac;
	tmb->t_request_within = req_within;
	tmb->t_request_outside = req_outside;
	tmb->t_request = request;
	tmb->new_dlg_uac = new_dlg_uac;
	tmb->dlg_response_uac = dlg_response_uac;
	tmb->new_dlg_uas = new_dlg_uas;
	tmb->dlg_request_uas = dlg_request_uas;
	tmb->free_dlg = free_dlg;
	tmb->print_dlg = print_dlg;

	return 1;
}


static int w_t_unref( struct sip_msg *foo, void *bar)
{
	return t_unref(foo);
}


static int script_init( struct sip_msg *foo, void *bar)
{
	/* we primarily reset all private memory here to make sure
	 * private values left over from previous message will
	 * not be used again */

	/* make sure the new message will not inherit previous
	 * message's t_on_negative value
	 */
	t_on_negative( 0 );
	t_on_reply(0);
	t_on_branch(0);
	/* reset the kr status */
	set_kr(0);
	return 1;
}


static int init_gf_mask( char* bf_mask )
{
	long long int l;
	char *end;

	if (bf_mask==0)
		return 0;

	errno = 0;
	/* ry bases 2, 10 and 16 */
	if (bf_mask[0]=='b' || bf_mask[0]=='B') {
		l = strtoll( bf_mask+1, &end, 2);
		if (*end==0 && errno==0)
			goto ok;
	}
	if (bf_mask[0]=='0' && bf_mask[1]=='x') {
		l = strtoll( bf_mask+2, &end, 16);
		if (*end==0 && errno==0)
			goto ok;
	}
	l = strtoll( bf_mask, &end, 10);
	if (*end==0 && errno==0)
		goto ok;

	return -1;
ok:
	gflags_mask = ~((unsigned int)l);
	DBG("DEBUG:tm:init_gf_mask: gflags_mask is %x\n",gflags_mask);
	return 0;
}


static int mod_init(void)
{
	DBG( "TM - (size of cell=%ld, sip_msg=%ld) initializing...\n", 
			(long)sizeof(struct cell), (long)sizeof(struct sip_msg));
	/* checking if we have sufficient bitmap capacity for given
	   maximum number of  branches */
	if (MAX_BRANCHES+1>31) {
		LOG(L_CRIT, "Too many max UACs for UAC branch_bm_t bitmap: %d\n",
			MAX_BRANCHES );
		return -1;
	}

	if (init_callid() < 0) {
		LOG(L_CRIT, "Error while initializing Call-ID generator\n");
		return -1;
	}

	if (register_fifo_cmd(fifo_uac, "t_uac_dlg", 0) < 0) {
		LOG(L_CRIT, "cannot register fifo t_uac\n");
		return -1;
	}

	if (register_fifo_cmd(fifo_uac_cancel, "t_uac_cancel", 0) < 0) {
		LOG(L_CRIT, "cannot register fifo t_uac_cancel\n");
		return -1;
	}

	if (register_fifo_cmd(fifo_hash, "t_hash", 0)<0) {
		LOG(L_CRIT, "cannot register hash\n");
		return -1;
	}

	if (register_fifo_cmd(fifo_t_reply, "t_reply", 0)<0) {
		LOG(L_CRIT, "cannot register t_reply\n");
		return -1;
	}

	if (unixsock_register_cmd("t_uac_dlg", unixsock_uac) < 0) {
		LOG(L_CRIT, "cannot register t_uac with the unix server\n");
		return -1;
	}

	if (unixsock_register_cmd("t_uac_cancel", unixsock_uac_cancel) < 0) {
		LOG(L_CRIT, "cannot register t_uac_cancel with the unix server\n");
		return -1;
	}

	if (unixsock_register_cmd("t_hash", unixsock_hash) < 0) {
		LOG(L_CRIT, "cannot register t_hash with the unix server\n");
		return -1;
	}

	if (unixsock_register_cmd("t_reply", unixsock_t_reply) < 0) {
		LOG(L_CRIT, "cannot register t_reply with the unix server\n");
		return -1;
	}

	/* building the hash table*/
	if (!init_hash_table()) {
		LOG(L_ERR, "ERROR: mod_init: initializing hash_table failed\n");
		return -1;
	}

	/* init static hidden values */
	init_t();

	if (!tm_init_timers()) {
		LOG(L_ERR, "ERROR: mod_init: timer init failed\n");
		return -1;
	}
	/* register the timer function */
	register_timer( timer_routine , 0 /* empty attr */, 1 );

	/* init_tm_stats calls process_count, which should
	 * NOT be called from mod_init, because one does not
	 * now, if a timer is used and thus how many processes
	 * will be started; however we started already our
	 * timers, so we know and process_count should not
	 * change any more
	 */
	if (init_tm_stats()<0) {
		LOG(L_CRIT, "ERROR: mod_init: failed to init stats\n");
		return -1;
	}

	if (uac_init()==-1) {
		LOG(L_ERR, "ERROR: mod_init: uac_init failed\n");
		return -1;
	}

	if (init_tmcb_lists()!=1) {
		LOG(L_CRIT, "ERROR:tm:mod_init: failed to init tmcb lists\n");
		return -1;
	}

	tm_init_tags();
	init_twrite_lines();
	if (init_twrite_sock() < 0) {
		LOG(L_ERR, "ERROR:tm:mod_init: Unable to create socket\n");
		return -1;
	}

	/* register post-script clean-up function */
	if (register_script_cb( w_t_unref, POST_SCRIPT_CB|REQ_TYPE_CB, 0)<0 ) {
		LOG(L_ERR,"ERROR:tm:mod_init: failed to register POST request "
			"callback\n");
		return -1;
	}
	if (register_script_cb( script_init, PRE_SCRIPT_CB|REQ_TYPE_CB , 0)<0 ) {
		LOG(L_ERR,"ERROR:tm:mod_init: failed to register PRE request "
			"callback\n");
		return -1;
	}

	if ( init_avp_params( fr_timer_param, fr_inv_timer_param)<0 ){
		LOG(L_ERR,"ERROR:tm:mod_init: failed to process timer AVPs\n");
		return -1;
	}

	if ( init_gf_mask( bf_mask_param )<0 ) {
		LOG(L_ERR,"ERROR:tm:mod_init: failed to process "
			"\"branch_flag_mask\" param\n");
		return -1;
	}
	return 0;
}


static int child_init(int rank)
{
	if (child_init_callid(rank) < 0) {
		LOG(L_ERR, "ERROR:tm:child_init: Error while initializing "
			"Call-ID generator\n");
		return -2;
	}

	return 0;
}




/**************************** wrapper functions ***************************/
static int t_check_status(struct sip_msg* msg, char *regexp, char *foo)
{
	regmatch_t pmatch;
	struct cell *t;
	char *status;
	char backup;
	int lowest_status;
	int n;

	/* first get the transaction */
	if (t_check( msg , 0 )==-1) return -1;
	if ( (t=get_t())==0) {
		LOG(L_ERR, "ERROR: t_check_status: cannot check status for a reply "
			"which has no T-state established\n");
		return -1;
	}
	backup = 0;

	switch (route_type) {
		case REQUEST_ROUTE:
			/* use the status of the last sent reply */
			status = int2str( t->uas.status, 0);
			break;
		case ONREPLY_ROUTE:
			/* use the status of the current reply */
			status = msg->first_line.u.reply.status.s;
			backup = status[msg->first_line.u.reply.status.len];
			status[msg->first_line.u.reply.status.len] = 0;
			break;
		case FAILURE_ROUTE:
			/* use the status of the winning reply */
			if (t_pick_branch( -1, 0, t, &lowest_status)<0 ) {
				LOG(L_CRIT,"BUG:t_check_status: t_pick_branch failed to get "
					" a final response in MODE_ONFAILURE\n");
				return -1;
			}
			status = int2str( lowest_status , 0);
			break;
		default:
			LOG(L_ERR,"ERROR:t_check_status: unsupported route_type %d\n",
					route_type);
			return -1;
	}

	DBG("DEBUG:t_check_status: checked status is <%s>\n",status);
	/* do the checking */
	n = regexec((regex_t*)regexp, status, 1, &pmatch, 0);

	if (backup) status[msg->first_line.u.reply.status.len] = backup;
	if (n!=0) return -1;
	return 1;
}


inline static int t_check_trans(struct sip_msg* msg, char *foo, char *bar)
{
	struct cell *trans;
	struct cell *bkup;
	int ret;

	if (msg->REQ_METHOD==METHOD_CANCEL) {
		/* parse needed hdrs*/
		if (check_transaction_quadruple(msg)==0) {
			LOG(L_ERR, "ERROR:tm:t_check_trans: too few headers\n");
			return 0; /*drop request!*/
		}
		if (!msg->hash_index)
			msg->hash_index = hash(msg->callid->body,get_cseq(msg)->number);
		/* performe lookup */
		trans = t_lookupOriginalT(  msg );
		if (trans) {
			UNREF( trans );
			return 1;
		} else {
			return -1;
		}
	} else {
		bkup = get_t();
		ret = t_lookup_request( msg , 0);
		if ( (trans=get_t())!=0 ) UNREF(trans);
		set_t( bkup );
		switch (ret) {
			case 1:
				/* transaction found */
				return 1;
			case -2:
				/* e2e ACK found */
				return 1;
			default:
				/* notfound */
				return -1;
		}
	}
	return ret;
}


static int t_flush_flags(struct sip_msg* msg, char *foo, char *bar)
{
	struct cell *t;

	/* first get the transaction */
	t = get_t();
	if ( t==0 || t==T_UNDEFINED) {
		LOG(L_ERR, "ERROR: t_flush_flags: cannot flush flags for a message "
			"which has no T-state established\n");
		return -1;
	}

	/* do the flush */
	t->uas.request->flags = msg->flags&gflags_mask;
	return 1;
}


inline static int t_local_replied(struct sip_msg* msg, char *all, char *bar)
{
	struct cell *t;
	int all_rpls;
	int i;

	if (t_check( msg , 0 )!=1) {
		LOG(L_ERR, "ERROR:t_local_replied: no transaction was set up\n");
		return -1;
	}
	t = get_t();

	all_rpls = (int)(long)all;

	/* is last reply local ? */
	if (t->relaied_reply_branch!=-2)
		return -1;
	/* were all replies local or none  */
	if (all_rpls) {
		for( i=t->first_branch ; i<t->nr_of_outgoings ; i++ ) {
			if (t->uac[i].flags&T_UAC_HAS_RECV_REPLY)
				return -1;
		}
	}

	return 1;
}


static int t_was_cancelled(struct sip_msg* msg, char *foo, char *bar)
{
	struct cell *t;

	/* first get the transaction */
	if (t_check( msg , 0 )==-1) return -1;
	if ( (t=get_t())==0) {
		LOG(L_ERR, "ERROR:tm:t_was_cancelled: cannot check cancel flag for "
			"a reply without a transaction\n");
		return -1;
	}
	return (t->flags&T_WAS_CANCELLED_FLAG)?1:-1;
}


inline static int w_t_check(struct sip_msg* msg, char* str, char* str2)
{
	return t_check( msg , 0  ) ? 1 : -1;
}


inline static int _w_t_forward_nonack(struct sip_msg* msg, char* proxy,
																	int proto)
{
	struct cell *t;
	if (t_check( msg , 0 )==-1) {
		LOG(L_ERR, "ERROR: forward_nonack: "
				"can't forward when no transaction was set up\n");
		return -1;
	}
	t=get_t();
	if ( t && t!=T_UNDEFINED ) {
		if (msg->REQ_METHOD==METHOD_ACK) {
			LOG(L_WARN,"WARNING: you don't really want to fwd hbh ACK\n");
			return -1;
		}
		return t_forward_nonack(t, msg, ( struct proxy_l *) proxy, proto );
	} else {
		DBG("DEBUG: forward_nonack: no transaction found\n");
		return -1;
	}
}


inline static int w_t_forward_nonack( struct sip_msg* msg, char* proxy,
										char* foo)
{
	return _w_t_forward_nonack(msg, proxy, PROTO_NONE);
}


inline static int w_t_forward_nonack_uri(struct sip_msg* msg, char *foo,
																	char *bar)
{
	return _w_t_forward_nonack(msg, 0, PROTO_NONE);
}


inline static int w_t_forward_nonack_udp( struct sip_msg* msg, char* proxy,
										char* foo)
{
	return _w_t_forward_nonack(msg, proxy, PROTO_UDP);
}


#ifdef USE_TCP
inline static int w_t_forward_nonack_tcp( struct sip_msg* msg, char* proxy,
										char* foo)
{
	return _w_t_forward_nonack(msg, proxy, PROTO_TCP);
}
#endif


#ifdef USE_TLS
inline static int w_t_forward_nonack_tls( struct sip_msg* msg, char* proxy,
										char* foo)
{
	return _w_t_forward_nonack(msg, proxy, PROTO_TLS);
}
#endif


inline static int w_t_reply(struct sip_msg* msg, char* str, char* str2)
{
	struct cell *t;

	if (msg->REQ_METHOD==METHOD_ACK) {
		LOG(L_WARN, "WARNING: t_reply: ACKs are not replied\n");
		return -1;
	}
	if (t_check( msg , 0 )==-1) return -1;
	t=get_t();
	if (!t) {
		LOG(L_ERR, "ERROR: t_reply: cannot send a t_reply to a message "
			"for which no T-state has been established\n");
		return -1;
	}
	/* if called from reply_route, make sure that unsafe version
	 * is called; we are already in a mutex and another mutex in
	 * the safe version would lead to a deadlock
	 */
	switch (route_type) {
		case FAILURE_ROUTE:
			DBG("DEBUG: t_reply_unsafe called from w_t_reply\n");
			return t_reply_unsafe(t, msg, (unsigned int)(long) str, str2);
		case REQUEST_ROUTE:
			return t_reply( t, msg, (unsigned int)(long) str, str2);
		default:
			LOG(L_CRIT, "BUG:tm:w_t_reply: unsupported route_type (%d)\n",
				route_type);
			return -1;
	}
}


inline static int w_t_release(struct sip_msg* msg, char* str, char* str2)
{
	struct cell *t;
	if (t_check( msg  , 0  )==-1) return -1;
	t=get_t();
	if ( t && t!=T_UNDEFINED ) 
		return t_release_transaction( t );
	return 1;
}


inline static int w_t_retransmit_reply( struct sip_msg* p_msg, char* foo, char* bar)
{
	struct cell *t;


	if (t_check( p_msg  , 0 )==-1) 
		return 1;
	t=get_t();
	if (t) {
		if (p_msg->REQ_METHOD==METHOD_ACK) {
			LOG(L_WARN, "WARNING: : ACKs transmit_replies not replied\n");
			return -1;
		}
		return t_retransmit_reply( t );
	} else 
		return -1;
}


inline static int w_t_newtran( struct sip_msg* p_msg, char* foo, char* bar ) 
{
	/* t_newtran returns 0 on error (negative value means
	   'transaction exists' */
	return t_newtran( p_msg );
}


inline static int w_t_on_negative( struct sip_msg* msg, char *go_to, char *foo)
{
	t_on_negative( (unsigned int )(long) go_to );
	return 1;
}


inline static int w_t_on_reply( struct sip_msg* msg, char *go_to, char *foo )
{
	t_on_reply( (unsigned int )(long) go_to );
	return 1;
}


inline static int w_t_on_branch( struct sip_msg* msg, char *go_to, char *foo )
{
	t_on_branch( (unsigned int )(long) go_to );
	return 1;
}


inline static int _w_t_relay_to( struct sip_msg  *p_msg, struct proxy_l *proxy)
{
	struct cell *t;

	if (route_type==FAILURE_ROUTE) {
		t=get_t();
		if (!t || t==T_UNDEFINED) {
			LOG(L_CRIT, "BUG: w_t_relay_to: undefined T\n");
			return -1;
		}
		if (t_forward_nonack(t, p_msg, proxy, PROTO_NONE)<=0 ) {
			LOG(L_ERR, "ERROR: w_t_relay_to: t_relay_to failed\n");
			return -1;
		}
		return 1;
	}

	if (route_type==REQUEST_ROUTE) {
		return t_relay_to( p_msg, proxy, PROTO_NONE,
			0 /* no replication */ );
	}

	LOG(L_CRIT, "ERROR: w_t_relay_to: unsupported route type: %d\n",
		route_type);
	return 0;
}


inline static int w_t_relay_to_udp( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	((struct proxy_l *)proxy)->proto=PROTO_UDP;
	return _w_t_relay_to( p_msg, ( struct proxy_l *) proxy);
}


#ifdef USE_TCP
inline static int w_t_relay_to_tcp( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	((struct proxy_l *)proxy)->proto=PROTO_TCP;
	return _w_t_relay_to( p_msg, ( struct proxy_l *) proxy);
}
#endif


#ifdef USE_TLS
inline static int w_t_relay_to_tls( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	((struct proxy_l *)proxy)->proto=PROTO_TLS;
	return _w_t_relay_to( p_msg, ( struct proxy_l *) proxy);
}
#endif


inline static int w_t_replicate( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, p_msg->rcv.proto );
}


inline static int w_t_replicate_udp( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, PROTO_UDP );
}


#ifdef USE_TCP
inline static int w_t_replicate_tcp( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, PROTO_TCP );
}
#endif


#ifdef USE_TLS
inline static int w_t_replicate_tls( struct sip_msg  *p_msg , 
	char *proxy, /* struct proxy_l *proxy expected */
	char *_foo       /* nothing expected */ )
{
	return t_replicate(p_msg, ( struct proxy_l *) proxy, PROTO_TLS );
}
#endif


inline static int w_t_relay( struct sip_msg  *p_msg , 
						char *_foo, char *_bar)
{
	return _w_t_relay_to( p_msg, 0);
}
