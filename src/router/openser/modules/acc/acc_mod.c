/*
 * $Id: acc_mod.c,v 1.10 2005/09/26 09:37:45 miconda Exp $
 * 
 * Accounting module
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
 * 2003-03-06: aligned to change in callback names (jiri)
 * 2003-03-06: fixed improper sql connection, now from 
 * 	           child_init (jiri)
 * 2003-03-11: New module interface (janakj)
 * 2003-03-16: flags export parameter added (janakj)
 * 2003-04-04  grand acc cleanup (jiri)
 * 2003-04-06: Opens database connection in child_init only (janakj)
 * 2003-04-24  parameter validation (0 t->uas.request) added (jiri)
 * 2003-11-04  multidomain support for mysql introduced (jiri)
 * 2003-12-04  global TM callbacks switched to per transaction callbacks
 *             (bogdan)
 * 2004-06-06  db cleanup: static db_url, calls to acc_db_{bind,init,close)
 *             (andrei)
 * 2005-05-30  acc_extra patch commited (ramona)
 * 2005-06-28  multi leg call support added (bogdan)
 */

#include <stdio.h>
#include <string.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../tm/t_hooks.h"
#include "../tm/tm_load.h"
#include "../tm/h_table.h"
#include "../../parser/msg_parser.h"
#include "../../parser/parse_from.h"

#include "acc_mod.h"
#include "acc.h"
#include "acc_extra.h"
#include "../tm/tm_load.h"

#ifdef RAD_ACC
#include <radiusclient-ng.h>
#include "dict.h"
#endif

#ifdef DIAM_ACC
#include "diam_dict.h"
#include "dict.h"
#include "diam_tcp.h"

#define M_NAME	"acc"
#endif

MODULE_VERSION

struct tm_binds tmb;

static int mod_init( void );
static void destroy(void);
static int child_init(int rank);


/* buffer used to read from TCP connection*/
#ifdef DIAM_ACC
rd_buf_t *rb;
#endif

/* ----- Parameter variables ----------- */


/* what would you like to report on */
/* should early media replies (183) be logged ? default==no */
int early_media = 0;
/* should failed replies (>=3xx) be logged ? default==no */
int failed_transaction_flag = 0;
/* would you like us to report CANCELs from upstream too? */
int report_cancels = 0;
/* report e2e ACKs too */
int report_ack = 1;

/* syslog flags, that need to be set for a transaction to 
 * be reported; 0=any, 1..MAX_FLAG otherwise */
int log_flag = 0;
int log_missed_flag = 0;
/* noisiness level logging facilities are used */
int log_level=L_NOTICE;
char *log_fmt=DEFAULT_LOG_FMT;
/* log extra variables */
static char *log_extra_str = 0;
struct acc_extra *log_extra = 0;
/* multi call-leg support */
int multileg_enabled = 0;
int src_avp_id = 0;
int dst_avp_id = 0;


#ifdef RAD_ACC
static char *radius_config = "/usr/local/etc/radiusclient/radiusclient.conf";
int radius_flag = 0;
int radius_missed_flag = 0;
static int service_type = -1;
void *rh;
struct attr attrs[A_MAX+MAX_ACC_EXTRA];
struct val vals[V_MAX];
/* rad extra variables */
static char *rad_extra_str = 0;
struct acc_extra *rad_extra = 0;
#endif

/* DIAMETER */
#ifdef DIAM_ACC
int diameter_flag = 1;
int diameter_missed_flag = 2;
char* diameter_client_host="localhost";
int diameter_client_port=3000;
/* diameter extra variables */
static char *dia_extra_str = 0;
struct acc_extra *dia_extra = 0;
#endif

#ifdef SQL_ACC
static char *db_url = 0; /* Database url */

/* sql flags, that need to be set for a transaction to 
 * be reported; 0=any, 1..MAX_FLAG otherwise; by default
 * set to the same values as syslog -> reporting for both
 * takes place
 */
int db_flag = 0;
int db_missed_flag = 0;
int db_localtime = 0;

char *db_table_acc="acc"; /* name of database table> */


/* names of columns in tables acc/missed calls*/
char* acc_sip_from_col   = "sip_from";
char* acc_sip_to_col     = "sip_to";
char* acc_sip_status_col = "sip_status";
char* acc_sip_method_col = "sip_method";
char* acc_i_uri_col      = "i_uri";
char* acc_o_uri_col      = "o_uri";
char* acc_totag_col      = "totag";
char* acc_fromtag_col    = "fromtag";
char* acc_domain_col     = "domain";
char* acc_from_uri       = "from_uri";
char* acc_to_uri         = "to_uri";
char* acc_sip_callid_col = "sip_callid";
char* acc_user_col       = "username";
char* acc_time_col       = "time";
char* acc_src_col        = "src_leg";
char* acc_dst_col        = "dst_leg";

/* db extra variables */
static char *db_extra_str = 0;
struct acc_extra *db_extra = 0;

/* name of missed calls table */
char *db_table_mc="missed_calls";
#endif

static int comment_fixup(void** param, int param_no);
static int w_acc_log_request(struct sip_msg *rq, char *comment, char *foo);
#ifdef SQL_ACC
static int w_acc_db_request(struct sip_msg *rq, char *comment, char *foo);
#endif
#ifdef RAD_ACC
static int w_acc_rad_request(struct sip_msg *rq, char *comment, char *foo);
#endif

#ifdef DIAM_ACC
static int w_acc_diam_request(struct sip_msg *rq, char *comment, char *foo);
#endif


static cmd_export_t cmds[] = {
	{"acc_log_request", w_acc_log_request, 1, comment_fixup,
			REQUEST_ROUTE|FAILURE_ROUTE},
#ifdef SQL_ACC
	{"acc_db_request",  w_acc_db_request,  2, comment_fixup,
			REQUEST_ROUTE|FAILURE_ROUTE},
#endif
#ifdef RAD_ACC
	{"acc_rad_request", w_acc_rad_request, 1, comment_fixup,
			REQUEST_ROUTE|FAILURE_ROUTE},
#endif
#ifdef DIAM_ACC
	{"acc_diam_request",w_acc_diam_request,1, comment_fixup,
			REQUEST_ROUTE|FAILURE_ROUTE},
#endif
	{0, 0, 0, 0, 0}
};



static param_export_t params[] = {
	{"early_media",             INT_PARAM, &early_media             },
	{"failed_transaction_flag", INT_PARAM, &failed_transaction_flag },
	{"report_ack",              INT_PARAM, &report_ack              },
	{"report_cancels",          INT_PARAM, &report_cancels          },
	{"multi_leg_enabled",       INT_PARAM, &multileg_enabled        },
	{"src_leg_avp_id",          INT_PARAM, &src_avp_id              },
	{"dst_leg_avp_id",          INT_PARAM, &dst_avp_id              },
	/* syslog specific */
	{"log_flag",             INT_PARAM, &log_flag             },
	{"log_missed_flag",      INT_PARAM, &log_missed_flag      },
	{"log_level",            INT_PARAM, &log_level            },
	{"log_fmt",              STR_PARAM, &log_fmt              },
	{"log_extra",            STR_PARAM, &log_extra_str        },
#ifdef RAD_ACC
	{"radius_config",        STR_PARAM, &radius_config        },
	{"radius_flag",          INT_PARAM, &radius_flag          },
	{"radius_missed_flag",   INT_PARAM, &radius_missed_flag   },
	{"service_type",         INT_PARAM, &service_type         },
	{"radius_extra",         STR_PARAM, &rad_extra_str        },
#endif
	/* DIAMETER specific */
#ifdef DIAM_ACC
	{"diameter_flag",        INT_PARAM, &diameter_flag        },
	{"diameter_missed_flag", INT_PARAM, &diameter_missed_flag },
	{"diameter_client_host", STR_PARAM, &diameter_client_host },
	{"diameter_client_port", INT_PARAM, &diameter_client_port },
	{"diameter_extra",       STR_PARAM, &dia_extra_str        },
#endif
	/* db-specific */
#ifdef SQL_ACC
	{"db_flag",              INT_PARAM, &db_flag              },
	{"db_missed_flag",       INT_PARAM, &db_missed_flag       },
	{"db_table_acc",         STR_PARAM, &db_table_acc         },
	{"db_table_missed_calls",STR_PARAM, &db_table_mc          },
	{"db_url",               STR_PARAM, &db_url               },
	{"db_localtime",         INT_PARAM, &db_localtime         },
	{"acc_sip_from_column",  STR_PARAM, &acc_sip_from_col     },
	{"acc_sip_to_column",    STR_PARAM, &acc_sip_to_col       },
	{"acc_sip_status_column",STR_PARAM, &acc_sip_status_col   },
	{"acc_sip_method_column",STR_PARAM, &acc_sip_method_col   },
	{"acc_i_uri_column",     STR_PARAM, &acc_i_uri_col        },
	{"acc_o_uri_column",     STR_PARAM, &acc_o_uri_col        },
	{"acc_sip_callid_column",STR_PARAM, &acc_sip_callid_col   },
	{"acc_user_column",      STR_PARAM, &acc_user_col         },
	{"acc_time_column",      STR_PARAM, &acc_time_col         },
	{"acc_from_uri_column",  STR_PARAM, &acc_from_uri         },
	{"acc_to_uri_column",    STR_PARAM, &acc_to_uri           },
	{"acc_totag_column",     STR_PARAM, &acc_totag_col        },
	{"acc_fromtag_column",   STR_PARAM, &acc_fromtag_col      },
	{"acc_domain_column",    STR_PARAM, &acc_domain_col       },
	{"acc_src_leg_column",   STR_PARAM, &acc_src_col          },
	{"acc_dst_leg_column",   STR_PARAM, &acc_dst_col          },
	{"db_extra",             STR_PARAM, &db_extra_str         },
#endif
	{0,0,0}
};


struct module_exports exports= {
	"acc",
	cmds,       /* exported functions */
	params,     /* exported params */
	mod_init,   /* initialization module */
	0,          /* response function */
	destroy,    /* destroy function */
	0,          /* oncancel function */
	child_init  /* per-child init function */
};



/* --------------- fixup function ---------------- */
static int comment_fixup(void** param, int param_no)
{
	str* s;

	if (param_no == 1) {
		s = (str*)pkg_malloc(sizeof(str));
		if (!s) {
			LOG(L_ERR, "ERROR:acc:comment_fixup: no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		s->s = (char*)*param;
		s->len = strlen(s->s);
		*param = (void*)s;
	}
	return 0;
}


/* ------------- Callback handlers --------------- */

static void acc_onreq( struct cell* t, int type, struct tmcb_params *ps );
static void tmcb_func( struct cell* t, int type, struct tmcb_params *ps );

/* --------------- function definitions -------------*/

static int verify_fmt(char *fmt) {

	if (!fmt) {
		LOG(L_ERR, "ERROR: verify_fmt: formatting string zero\n");
		return -1;
	}
	if (!(*fmt)) {
		LOG(L_ERR, "ERROR: verify_fmt: formatting string empty\n");
		return -1;
	}
	if (strlen(fmt)>ALL_LOG_FMT_LEN) {
		LOG(L_ERR, "ERROR: verify_fmt: formatting string too long\n");
		return -1;
	}

	while(*fmt) {
		if (!strchr(ALL_LOG_FMT,*fmt)) {
			LOG(L_ERR, "ERROR: verify_fmt: char in log_fmt invalid: %c\n", 
				*fmt);
			return -1;
		}
		fmt++;
	}
	return 1;
}


static int mod_init( void )
{
#ifdef RAD_ACC
	int nr_extra_rad;
#endif

	LOG(L_INFO,"ACC - initializing\n");

	/* load the TM API */
	if (load_tm_api(&tmb)!=0) {
		LOG(L_ERR, "ERROR:acc:mod_init: can't load TM API\n");
		return -1;
	}

	if (verify_fmt(log_fmt)==-1) return -1;

	/* register callbacks*/
	/* listen for all incoming requests  */
	if ( tmb.register_tmcb( 0, 0, TMCB_REQUEST_IN, acc_onreq, 0 ) <=0 ) {
		LOG(L_ERR,"ERROR:acc:mod_init: cannot register TMCB_REQUEST_IN "
			"callback\n");
		return -1;
	}

	if (multileg_enabled && (dst_avp_id==0 || src_avp_id==0) ) {
		LOG(L_ERR,"ERROR:acc:mod_init: multi call-leg enabled but no src "
			" and dst avp IDs defined!\n");
		return -1;
	}

	/* init the extra engine */
	init_acc_extra();

	/* parse the extra string, if any */
	if (log_extra_str && (log_extra=parse_acc_extra(log_extra_str))==0 ) {
		LOG(L_ERR,"ERROR:acc:mod_init: failed to parse log_extra param\n");
		return -1;
	}

#ifdef SQL_ACC
	if (db_url && db_url[0]) {
		if (acc_db_bind(db_url)<0){
			LOG(L_ERR, "ERROR:acc:mod_init: acc_db_init: failed..."
				"did you load a database module?\n");
			return -1;
		}
		/* parse the extra string, if any */
		if (db_extra_str && (db_extra=parse_acc_extra(db_extra_str))==0 ) {
			LOG(L_ERR,"ERROR:acc:mod_init: failed to parse db_extra param\n");
			return -1;
		}
	} else {
		db_url = 0;
	}
#endif

#ifdef RAD_ACC
	/* parse the extra string, if any */
	if (rad_extra_str && (rad_extra=parse_acc_extra(rad_extra_str))==0 ) {
		LOG(L_ERR,"ERROR:acc:mod_init: failed to parse rad_extra param\n");
		return -1;
	}

	memset(attrs, 0, sizeof(attrs));
	memset(attrs, 0, sizeof(vals));
	attrs[A_CALLING_STATION_ID].n			= "Calling-Station-Id";
	attrs[A_CALLED_STATION_ID].n			= "Called-Station-Id";
	attrs[A_SIP_TRANSLATED_REQUEST_URI].n	= "Sip-Translated-Request-URI";
	attrs[A_ACCT_SESSION_ID].n		= "Acct-Session-Id";
	attrs[A_SIP_TO_TAG].n			= "Sip-To-Tag";
	attrs[A_SIP_FROM_TAG].n			= "Sip-From-Tag";
	attrs[A_SIP_CSEQ].n				= "Sip-CSeq";
	attrs[A_ACCT_STATUS_TYPE].n		= "Acct-Status-Type";
	attrs[A_SERVICE_TYPE].n			= "Service-Type";
	attrs[A_SIP_RESPONSE_CODE].n	= "Sip-Response-Code";
	attrs[A_SIP_METHOD].n			= "Sip-Method";
	attrs[A_USER_NAME].n			= "User-Name";
	if (multileg_enabled) {
		attrs[A_SRC_LEG].n			= "Sip-Leg-Source";
		attrs[A_DST_LEG].n			= "Sip-Leg-Destination";
	}
	vals[V_STATUS_START].n			= "Start";
	vals[V_STATUS_STOP].n			= "Stop";
	vals[V_STATUS_FAILED].n			= "Failed";
	vals[V_SIP_SESSION].n			= "Sip-Session";
	/* add and count the extras as attributes */
	nr_extra_rad = extra2attrs( rad_extra, attrs, A_MAX);

	/* read config */
	if ((rh = rc_read_config(radius_config)) == NULL) {
		LOG(L_ERR, "ERROR: acc: error opening radius config file: %s\n", 
			radius_config );
		return -1;
	}
	/* read dictionary */
	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary"))!=0) {
		LOG(L_ERR, "ERROR: acc: error reading radius dictionary\n");
		return -1;
	}

	INIT_AV(rh, attrs, A_MAX+nr_extra_rad, vals, "acc", -1, -1);

	if (service_type != -1)
		vals[V_SIP_SESSION].v = service_type;
#endif

#ifdef DIAM_ACC
	/* parse the extra string, if any */
	if (dia_extra_str && (dia_extra=parse_acc_extra(dia_extra_str))==0 ) {
		LOG(L_ERR,"ERROR:acc:mod_init: failed to parse dia_extra param\n");
		return -1;
	}

	if (extra2int(dia_extra)!=0) {
		LOG(L_ERR,"ERROR:acc:mod_init: extar names for DIAMTER must be "
			" integer AVP codes\n");
		return -1;
	}
#endif
	return 0;
}



static int child_init(int rank)
{
#ifdef SQL_ACC
	if (db_url && acc_db_init(db_url)<0)
		return -1;
#endif

	/* DIAMETER */
#ifdef DIAM_ACC
	/* open TCP connection */
	DBG(M_NAME": Initializing TCP connection\n");

	sockfd = init_mytcp(diameter_client_host, diameter_client_port);
	if(sockfd==-1) 
	{
		DBG(M_NAME": TCP connection not established\n");
		return -1;
	}

	DBG(M_NAME": TCP connection established on sockfd=%d\n", sockfd);

	/* every child with its buffer */
	rb = (rd_buf_t*)pkg_malloc(sizeof(rd_buf_t));
	if(!rb)
	{
		DBG("acc: mod_child_init: no more free memory\n");
		return -1;
	}
	rb->buf = 0;

#endif

	return 0;
}



static void destroy(void)
{
	if (log_extra)
		destroy_extras( log_extra);
#ifdef SQL_ACC
	acc_db_close();
	if (db_extra)
		destroy_extras( db_extra);
#endif
#ifdef RAD_ACC
	if (rad_extra)
		destroy_extras( rad_extra);
#endif
#ifdef DIAM_ACC
	close_tcp_connection(sockfd);
	if (dia_extra)
		destroy_extras( dia_extra);
#endif
}



static inline void acc_preparse_req(struct sip_msg *rq)
{
	/* try to parse from for From-tag for accounted transactions; 
	 * don't be worried about parsing outcome -- if it failed, 
	 * we will report N/A
	 */
	parse_headers(rq, HDR_CALLID_F| HDR_FROM_F| HDR_TO_F, 0 );
	parse_from_header(rq);

	if (strchr(log_fmt, 'p') || strchr(log_fmt, 'D')) {
		parse_orig_ruri(rq);
	}
}



/* prepare message and transaction context for later accounting */
static void acc_onreq( struct cell* t, int type, struct tmcb_params *ps )
{
	int tmcb_types;

	if (is_acc_on(ps->req) || is_mc_on(ps->req)) {
		/* install additional handlers */
		tmcb_types =
			/* report on completed transactions */
			TMCB_RESPONSE_OUT |
			/* account e2e acks if configured to do so */
			TMCB_E2EACK_IN |
			/* report on missed calls
			TMCB_ON_FAILURE | */
			/* get incoming replies ready for processing */
			TMCB_RESPONSE_IN;
		if (tmb.register_tmcb( 0, t, tmcb_types, tmcb_func, 0 )<=0) {
			LOG(L_ERR,"ERROR:acc:acc_onreq: cannot register additional "
				"callbacks\n");
			return;
		}
		/* do some parsing in advance */
		acc_preparse_req(ps->req);
		/* also, if that is INVITE, disallow silent t-drop */
		if (ps->req->REQ_METHOD==METHOD_INVITE) {
			DBG("DEBUG: noisy_timer set for accounting\n");
			t->flags |= T_NOISY_CTIMER_FLAG;
		}
	}
}


/* is this reply of interest for accounting ? */
static inline int should_acc_reply(struct cell *t, int code)
{
	struct sip_msg *r;

	r=t->uas.request;

	/* validation */
	if (r==0) {
		LOG(L_ERR, "ERROR: acc: should_acc_reply: 0 request\n");
		return 0;
	}

	/* negative transactions reported otherwise only if explicitly 
	 * demanded */
	if (!is_failed_acc_on(r) && code >=300) return 0;
	if (!is_acc_on(r))
		return 0;
	if (skip_cancel(r))
		return 0;
	if (code < 200 && ! (early_media && code==183))
		return 0;

	return 1; /* seed is through, we will account this reply */
}

/* parse incoming replies before cloning */
static inline void acc_onreply_in(struct cell *t, struct sip_msg *reply,
	int code, void *param)
{
	/* validation */
	if (t->uas.request==0) {
		LOG(L_ERR, "ERROR: acc: should_acc_reply: 0 request\n");
		return;
	}

	/* don't parse replies in which we are not interested */
	/* missed calls enabled ? */
	if (((is_invite(t) && code>=300 && is_mc_on(t->uas.request))
					|| should_acc_reply(t,code)) 
				&& (reply && reply!=FAKED_REPLY)) {
		parse_headers(reply, HDR_TO_F, 0 );
	}
}

/* initiate a report if we previously enabled MC accounting for this t */
static inline void on_missed(struct cell *t, struct sip_msg *reply,
	int code, void *param )
{
	int reset_lmf; 
#ifdef SQL_ACC
	int reset_dmf;
#endif
#ifdef RAD_ACC
	int reset_rmf;
#endif
/* DIAMETER */
#ifdef DIAM_ACC
	int reset_dimf;
#endif

	/* validation */
	if (t->uas.request==0) {
		DBG("DBG: acc: on_missed: no uas.request, local t; skipping\n");
		return;
	}

	if (is_invite(t) && code>=300) {
		if (is_log_mc_on(t->uas.request)) {
			acc_log_missed( t, reply, code);
			reset_lmf=1;
		} else reset_lmf=0;
#ifdef SQL_ACC
		if (db_url && is_db_mc_on(t->uas.request)) {
			acc_db_missed( t, reply, code);
			reset_dmf=1;
		} else reset_dmf=0;
#endif
#ifdef RAD_ACC
		if (is_rad_mc_on(t->uas.request)) {
			acc_rad_missed(t, reply, code );
			reset_rmf=1;
		} else reset_rmf=0;
#endif
/* DIAMETER */
#ifdef DIAM_ACC
		if (is_diam_mc_on(t->uas.request)) {
			acc_diam_missed(t, reply, code );
			reset_dimf=1;
		} else reset_dimf=0;
#endif
		/* we report on missed calls when the first
		 * forwarding attempt fails; we do not wish to
		 * report on every attempt; so we clear the flags; 
		 * we do it after all reporting is over to be sure
		 * that all reporting functions got a fair chance
		 */
		if (reset_lmf) resetflag(t->uas.request, log_missed_flag);
#ifdef SQL_ACC
		if (reset_dmf) resetflag(t->uas.request, db_missed_flag);
#endif
#ifdef RAD_ACC
		if (reset_rmf) resetflag(t->uas.request, radius_missed_flag);
#endif
/* DIAMETER */
#ifdef DIAM_ACC
		if (reset_dimf) resetflag(t->uas.request, diameter_missed_flag);
#endif
	}
}


/* initiate a report if we previously enabled accounting for this t */
static inline void acc_onreply( struct cell* t, struct sip_msg *reply,
	int code, void *param )
{
	/* validation */
	if (t->uas.request==0) {
		DBG("DBG: acc: onreply: no uas.request, local t; skipping\n");
		return;
	}

	/* acc_onreply is bound to TMCB_REPLY which may be called
	   from _reply, like when FR hits; we should not miss this
	   event for missed calls either */
	on_missed(t, reply, code, param );

	if (!should_acc_reply(t, code)) return;
	if (is_log_acc_on(t->uas.request))
		acc_log_reply(t, reply, code);
#ifdef SQL_ACC
	if (db_url && is_db_acc_on(t->uas.request))
		acc_db_reply(t, reply, code);
#endif
#ifdef RAD_ACC
	if (is_rad_acc_on(t->uas.request))
		acc_rad_reply(t, reply, code);
#endif
/* DIAMETER */
#ifdef DIAM_ACC
	if (is_diam_acc_on(t->uas.request))
		acc_diam_reply(t, reply, code);
#endif
}




static inline void acc_onack( struct cell* t , struct sip_msg *ack,
	int code, void *param )
{
	/* only for those guys who insist on seeing ACKs as well */
	if (!report_ack) return;
	/* if acc enabled for flagged transaction, check if flag matches */
	if (is_log_acc_on(t->uas.request)) {
		acc_preparse_req(ack);
		acc_log_ack(t, ack);
	}
#ifdef SQL_ACC
	if (db_url && is_db_acc_on(t->uas.request)) {
		acc_preparse_req(ack);
		acc_db_ack(t, ack);
	}
#endif
#ifdef RAD_ACC
	if (is_rad_acc_on(t->uas.request)) {
		acc_preparse_req(ack);
		acc_rad_ack(t,ack);
	}
#endif
/* DIAMETER */
#ifdef DIAM_ACC
	if (is_diam_acc_on(t->uas.request)) {
		acc_preparse_req(ack);
		acc_diam_ack(t,ack);
	}
#endif
	
}


static void tmcb_func( struct cell* t, int type, struct tmcb_params *ps )
{
	if (type&TMCB_RESPONSE_OUT) {
		acc_onreply( t, ps->rpl, ps->code, ps->param );
	} else if (type&TMCB_E2EACK_IN) {
		acc_onack( t, ps->req, ps->code, ps->param );
	/*} else if (type&TMCB_ON_FAILURE) {
		on_missed( t, ps->rpl, ps->code, ps->param );*/
	} else if (type&TMCB_RESPONSE_IN) {
		acc_onreply_in( t, ps->rpl, ps->code, ps->param);
	}
}


/* these wrappers parse all what may be needed; they don't care about
 * the result -- accounting functions just display "unavailable" if there
 * is nothing meaningful
 */
static int w_acc_log_request(struct sip_msg *rq, char *comment, char *foo)
{
	str txt;

	txt.s=ACC_REQUEST;
	txt.len=ACC_REQUEST_LEN;
	acc_preparse_req(rq);
	return acc_log_request(rq, rq->to, &txt, (str*)comment);
}


#ifdef SQL_ACC
static int w_acc_db_request(struct sip_msg *rq, char *comment, char *table)
{
	if (!db_url) {
		LOG(L_ERR,"ERROR:acc:w_acc_db_request: DB support not configured\n");
		return -1;
	}
	acc_preparse_req(rq);
	return acc_db_request(rq, rq->to, (str*)comment, table, SQL_MC_FMT );
}
#endif


#ifdef RAD_ACC
static int w_acc_rad_request(struct sip_msg *rq, char *comment, 
				char *foo)
{
	acc_preparse_req(rq);
	return acc_rad_request(rq, rq->to, (str*)comment);
}
#endif


/* DIAMETER */
#ifdef DIAM_ACC
static int w_acc_diam_request(struct sip_msg *rq, char *comment, 
				char *foo)
{
	str phrase;

	phrase.s=comment;
	phrase.len=strlen(comment);	/* fix_param would be faster! */
	acc_preparse_req(rq);
	return acc_diam_request(rq, rq->to, (str*)comment);
}
#endif

