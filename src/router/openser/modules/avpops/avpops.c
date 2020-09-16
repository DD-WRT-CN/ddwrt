/*
 * $Id: avpops.c,v 1.14 2005/09/12 17:20:10 miconda Exp $
 *
 * Copyright (C) 2004 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router.
 *
 * AVPOPS OpenSER-module is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * AVPOPS OpenSER-module is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * For any questions about this software and its license, please contact
 * Voice Sistem at following e-mail address:
 *         office@voice-sistem.ro
 *
 *
 * History:
 * ---------
 *  2004-10-04  first version (ramona)
 *  2004-11-15  added support for db schemes for avp_db_load (ramona)
 *  2004-11-17  aligned to new AVP core global aliases (ramona)
 *  2005-01-30  "fm" (fast match) operator added (ramona)
 *  2005-01-30  avp_copy (copy/move operation) added (ramona)
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* for regex */
#include <regex.h>

#include "../../mem/shm_mem.h"
#include "../../mem/mem.h"
#include "../../parser/parse_hname2.h"
#include "../../sr_module.h"
#include "../../str.h"
#include "../../dprint.h"
#include "../../error.h"
#include "avpops_parse.h"
#include "avpops_impl.h"
#include "avpops_db.h"


MODULE_VERSION

/* modules param variables */
static char *DB_URL        = 0;  /* database url */
static char *DB_TABLE      = 0;  /* table */
static int  use_domain     = 0;  /* if domain should be use for avp matching */
static char *db_columns[6] = {"uuid","attribute","value",
                              "type","username","domain"};


static int avpops_init(void);
static int avpops_child_init(int rank);

static int register_galiases( modparam_t type, void* val);
static int fixup_db_load_avp(void** param, int param_no);
static int fixup_db_delete_avp(void** param, int param_no);
static int fixup_db_store_avp(void** param, int param_no);
static int fixup_write_avp(void** param, int param_no);
static int fixup_delete_avp(void** param, int param_no);
static int fixup_pushto_avp(void** param, int param_no);
static int fixup_check_avp(void** param, int param_no);
static int fixup_copy_avp(void** param, int param_no);
static int fixup_printf(void** param, int param_no);
static int fixup_subst(void** param, int param_no);
static int fixup_op_avp(void** param, int param_no);
static int fixup_is_avp_set(void** param, int param_no);

static int w_dbload_avps(struct sip_msg* msg, char* source, char* param);
static int w_dbstore_avps(struct sip_msg* msg, char* source, char* param);
static int w_dbdelete_avps(struct sip_msg* msg, char* source, char* param);
static int w_write_avps(struct sip_msg* msg, char* source, char* param);
static int w_delete_avps(struct sip_msg* msg, char* param, char *foo);
static int w_pushto_avps(struct sip_msg* msg, char* destination, char *param);
static int w_check_avps(struct sip_msg* msg, char* param, char *check);
static int w_copy_avps(struct sip_msg* msg, char* param, char *check);
static int w_print_avps(struct sip_msg* msg, char* foo, char *bar);
static int w_printf(struct sip_msg* msg, char* dest, char *format);
static int w_subst(struct sip_msg* msg, char* src, char *subst);
static int w_op_avps(struct sip_msg* msg, char* param, char *op);
static int w_is_avp_set(struct sip_msg* msg, char* param, char *foo);


/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"avp_db_load",   w_dbload_avps,  2, fixup_db_load_avp,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_db_store",  w_dbstore_avps,  2, fixup_db_store_avp,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_db_delete", w_dbdelete_avps, 2, fixup_db_delete_avp,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_write",  w_write_avps,  2, fixup_write_avp,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_delete", w_delete_avps, 1, fixup_delete_avp,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_pushto", w_pushto_avps, 2, fixup_pushto_avp,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_check",  w_check_avps, 2, fixup_check_avp,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_copy",   w_copy_avps,  2,  fixup_copy_avp,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_print",  w_print_avps, 0, 0,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_printf", w_printf,  2, fixup_printf,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_subst",  w_subst,   2, fixup_subst,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"avp_op",     w_op_avps, 2, fixup_op_avp,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{"is_avp_set", w_is_avp_set, 1, fixup_is_avp_set,
							REQUEST_ROUTE|FAILURE_ROUTE|BRANCH_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"avp_url",           STR_PARAM, &DB_URL         },
	{"avp_table",         STR_PARAM, &DB_TABLE       },
	{"avp_aliases",       STR_PARAM|USE_FUNC_PARAM, (void*)register_galiases },
	{"use_domain",        INT_PARAM, &use_domain     },
	{"uuid_column",       STR_PARAM, &db_columns[0]  },
	{"attribute_column",  STR_PARAM, &db_columns[1]  },
	{"value_column",      STR_PARAM, &db_columns[2]  },
	{"type_column",       STR_PARAM, &db_columns[3]  },
	{"username_column",   STR_PARAM, &db_columns[4]  },
	{"domain_column",     STR_PARAM, &db_columns[5]  },
	{"db_scheme",         STR_PARAM|USE_FUNC_PARAM, (void*)avp_add_db_scheme },
	{0, 0, 0}
};


struct module_exports exports = {
	"avpops",
	cmds,     /* Exported functions */
	params,   /* Exported parameters */
	avpops_init, /* Module initialization function */
	(response_function) 0,
	(destroy_function) 0,
	0,
	(child_init_function) avpops_child_init /* per-child init function */
};



static int register_galiases( modparam_t type, void* val)
{
	
	if (val!=0 && ((char*)val)[0]!=0)
	{
		if ( add_avp_galias_str((char*)val)!=0 )
			return -1;
	}

	return 0;
}


static int avpops_init(void)
{
	LOG(L_INFO,"AVPops - initializing\n");

	/* if DB_URL defined -> bind to a DB module */
	if (DB_URL!=0)
	{
		/* check AVP_TABLE param */
		if (DB_TABLE==0)
		{
			LOG(L_CRIT,"ERROR:avpops_init: \"AVP_DB\" present but "
				"\"AVP_TABLE\" found empty\n");
			goto error;
		}
		/* bind to the DB module */
		if (avpops_db_bind(DB_URL)<0)
			goto error;
	}

	init_store_avps( db_columns );

	return 0;
error:
	return -1;
}


static int avpops_child_init(int rank)
{
	/* init DB only if enabled */
	if (DB_URL==0)
		return 0;
	/* skip main process and TCP manager process */
	if (rank==PROC_MAIN || rank==PROC_TCP_MAIN)
		return 0;
	/* init DB connection */
	return avpops_db_init(DB_URL, DB_TABLE, db_columns);
}



static struct fis_param *get_attr_or_alias(char *s)
{
	struct fis_param *ap;
	char *p;
	int type;
	str alias;

	/* compose the param structure */
	ap = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
	if (ap==0)
	{
		LOG(L_ERR,"ERROR:avpops:get_attr_or_alias: no more pkg mem\n");
		goto error;
	}
	memset( ap, 0, sizeof(struct fis_param));

	if (*s=='$')
	{
		/* alias */
		alias .s = s+1;
		alias.len = strlen(alias.s);
		if (lookup_avp_galias( &alias, &type, &ap->val)==-1)
		{
			LOG(L_ERR,"ERROR:avpops:get_attr_or_alias: unknow alias"
				"\"%s\"\n", s+1);
			goto error;
		}
		ap->opd |= (type&AVP_NAME_STR)?AVPOPS_VAL_STR:AVPOPS_VAL_INT;
	} else {
		if ( (p=parse_avp_attr( s, ap, 0))==0 || *p!=0)
		{
			LOG(L_ERR,"ERROR:avpops:get_attr_or_alias: failed to parse "
				"attribute name <%s>\n", s);
			goto error;
		}
	}
	ap->opd |= AVPOPS_VAL_AVP;
	return ap;
error:
	return 0;
}


static int fixup_db_avp(void** param, int param_no, int allow_scheme)
{
	struct fis_param *sp;
	struct db_param  *dbp;
	int flags;
	int flags0;
	str alias;
	char *s;
	char *p;

	flags=0;
	flags0=0;
	if (DB_URL==0)
	{
		LOG(L_ERR,"ERROR:avpops:fixup_db_avp: you have to config a db url "
			"for using avp_db_xxx functions\n");
		return E_UNSPEC;
	}

	s = (char*)*param;
	if (param_no==1)
	{
		/* prepare the fis_param structure */
		sp = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
		if (sp==0) {
			LOG(L_ERR,"ERROR:avpops:fixup_db_avp: no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset( sp, 0, sizeof(struct fis_param));

		if ( (p=strchr(s,'/'))!=0)
		{
			*(p++) = 0;
			/* check for extra flags/params */
			if (!strcasecmp("domain",p)) {
				flags|=AVPOPS_FLAG_DOMAIN0;
			} else if (!strcasecmp("username",p)) {
				flags|=AVPOPS_FLAG_USER0;
			} else if (!strcasecmp("uri",p)) {
				flags|=AVPOPS_FLAG_URI0;
			} else if (!strcasecmp("uuid",p)) {
				flags|=AVPOPS_FLAG_UUID0;
			} else {
				LOG(L_ERR,"ERROR:avpops:fixup_db_avp: unknow flag "
					"<%s>\n",p);
				return E_UNSPEC;
			}
		}
		if (*s!='$')
		{
			/* is a constant string -> use it as uuid*/
			sp->opd = ((flags==0)?AVPOPS_FLAG_UUID0:flags)|AVPOPS_VAL_STR;
			sp->val.s = (str*)pkg_malloc(strlen(s)+1+sizeof(str));
			if (sp->val.s==0) {
				LOG(L_ERR,"ERROR:avpops:fixup_db_avp: no more pkg mem\n");
				return E_OUT_OF_MEM;
			}
			sp->val.s->s = ((char*)sp->val.s) + sizeof(str);
			sp->val.s->len = strlen(s);
			strcpy(sp->val.s->s,s);
		} else {
			/* is a variable $xxxxx */
			s++;
			if(flags==0)
				flags0=AVPOPS_FLAG_URI0;
			if ( (!strcasecmp( "from", s) && (flags|=AVPOPS_USE_FROM)) 
					|| (!strcasecmp( "to", s) && (flags|=AVPOPS_USE_TO)) 
					|| (!strcasecmp( "ruri", s) && (flags|=AVPOPS_USE_RURI)) )
			{
				memset(sp, 0, sizeof(struct fis_param));
				sp->opd = flags0|flags|AVPOPS_VAL_NONE;
			} else {
				/* can be only an AVP alias */
				alias .s = s;
				alias.len = strlen(alias.s);
				flags0=0;
				if (lookup_avp_galias( &alias, &flags0, &sp->val)==-1 )
				{
					LOG(L_ERR,"ERROR:avpops:fixup_db_avp: source/flags \"%s\""
						" unknown!\n",s);
					return E_UNSPEC;
				}
				sp->opd = ((flags==0)?AVPOPS_FLAG_UUID0:flags)|AVPOPS_VAL_AVP |
					((flags0&AVP_NAME_STR)?AVPOPS_VAL_STR:AVPOPS_VAL_INT);
			}
		}
		pkg_free(*param);
		*param=(void*)sp;
	} else if (param_no==2) {
		/* compose the db_param structure */
		dbp = (struct db_param*)pkg_malloc(sizeof(struct db_param));
		if (dbp==0)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_db_avp: no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset( dbp, 0, sizeof(struct db_param));
		if ( parse_avp_db( s, dbp, allow_scheme)!=0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_db_avp: parse failed\n");
			return E_UNSPEC;
		}
		pkg_free(*param);
		*param=(void*)dbp;
	}

	return 0;
}


static int fixup_db_load_avp(void** param, int param_no)
{
	return fixup_db_avp( param, param_no, 1/*allow scheme*/);
}


static int fixup_db_delete_avp(void** param, int param_no)
{
	return fixup_db_avp( param, param_no, 0/*no scheme*/);
}


static int fixup_db_store_avp(void** param, int param_no)
{
	return fixup_db_avp( param, param_no, 0/*no scheme*/);
}


static int fixup_write_avp(void** param, int param_no)
{
	struct hdr_field hdr;
	struct fis_param *ap;
	int  flags;
	int  len;
	char *s;
	char *p;

	flags=0;
	s = (char*)*param;
	ap = 0 ;
	
	if (param_no==1)
	{
		if ( *s=='$' )
		{
			/* is variable */
			if ((++s)==0)
			{
				LOG(L_ERR,"ERROR:avops:fixup_write_avp: bad param 1; "
					"expected : $[from|to|ruri|hdr] or int/str value\n");
				return E_UNSPEC;
			}
			if ( (p=strchr(s,'/'))!=0)
				*(p++) = 0;
			if ( (!strcasecmp( "from", s) && (flags|=AVPOPS_USE_FROM))
				|| (!strcasecmp( "to", s) && (flags|=AVPOPS_USE_TO))
				|| (!strcasecmp( "ruri", s) && (flags|=AVPOPS_USE_RURI))
				|| (!strcasecmp( "duri", s) && (flags|=AVPOPS_USE_DURI))
				|| (!strcasecmp( "dst_ip", s) && (flags|=AVPOPS_USE_DST_IP))
				|| (!strcasecmp( "src_ip", s) && (flags|=AVPOPS_USE_SRC_IP))
				|| (!strncasecmp( "hdr", s, 3) && (flags|=AVPOPS_USE_HDRREQ)) )
			{
				ap = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
				if (ap==0)
				{
					LOG(L_ERR,"ERROR:avpops:fixup_write_avp: no more "
						"pkg mem\n");
					return E_OUT_OF_MEM;
				}
				memset( ap, 0, sizeof(struct fis_param));
				/* any falgs ? */
				if ( p && !(!(flags&(AVPOPS_USE_SRC_IP|AVPOPS_USE_HDRREQ
				|AVPOPS_USE_DST_IP)) && (
				(!strcasecmp("username",p) && (flags|=AVPOPS_FLAG_USER0)) ||
				(!strcasecmp("domain", p) && (flags|=AVPOPS_FLAG_DOMAIN0)))) )
				{
					LOG(L_ERR,"ERROR:avpops:fixup_write_avp: flag \"%s\""
						" unknown!\n", p);
					return E_UNSPEC;
				}
				if (flags&AVPOPS_USE_HDRREQ)
				{
					len = strlen(s);
					if (len<6 || 
						((s[3]!='[' || s[len-1]!=']')
							&& (s[3]!='(' || s[len-1]!=')')))
					{
						LOG(L_ERR,"ERROR:avpops:fixup_write_avp: invalid hdr "
							"specification \"%s\"\n",s);
						return E_UNSPEC;
					}
					s[len-1] = ':';
					/* parse header name */
					if (parse_hname2( s+4, s+len, &hdr)==0) {
						LOG(L_ERR,"BUG:avpops:fixup_write_avp: parse header "
							"failed\n");
						return E_UNSPEC;
					}
					if (hdr.type==HDR_OTHER_T) {
						/* duplicate hdr name */
						len -= 5; /*hdr[]*/
						ap->val.s = (str*)pkg_malloc(sizeof(str)+len+1);
						if (ap->val.s==0)
						{
							LOG(L_ERR,"ERROR:avpops:fixup_write_avp: no more "
								"pkg mem\n");
							return E_OUT_OF_MEM;
						}
						ap->val.s->s = ((char*)(ap->val.s)) + sizeof(str);
						ap->val.s->len = len;
						memcpy( ap->val.s->s, s+4, len);
						ap->val.s->s[len] = 0;
						DBG("DEBUF:avpops:fixup_write_avp: hdr=<%s>\n",
							ap->val.s->s);
					} else {
						ap->val.n = hdr.type;
						flags |= AVPOPS_VAL_INT;
					}
				}
				ap->opd = flags|AVPOPS_VAL_NONE;
			} else {
				LOG(L_ERR,"ERROR:avpops:fixup_write_avp: source \"%s\""
					" unknown!\n", s);
				return E_UNSPEC;
			}
		} else {
			/* is value */
			if ( (ap=parse_intstr_value(s,strlen(s)))==0 )
			{
				LOG(L_ERR,"ERROR:avops:fixup_write_avp: bad param 1; "
					"expected : $[from|to|ruri] or int/str value\n");
				return E_UNSPEC;
			}
		}
	} else if (param_no==2) {
		if ( (ap=get_attr_or_alias(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_write_avp: bad attribute name"
				"/alias <%s>\n", s);
			return E_UNSPEC;
		}
		/* attr name is mandatory */
		if (ap->opd&AVPOPS_VAL_NONE)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_write_avp: you must specify "
				"a name for the AVP\n");
			return E_UNSPEC;
		}
	}

	pkg_free(*param);
	*param=(void*)ap;
	return 0;
}


static int fixup_delete_avp(void** param, int param_no)
{
	struct fis_param *ap;
	char *p;

	if (param_no==1) {
		/* attribute name / alias */
		if ( (p=strchr((char*)*param,'/'))!=0 )
			*(p++)=0;
		if ( (ap=get_attr_or_alias((char*)*param))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_delete_avp: bad attribute name"
				"/alias <%s>\n", (char*)*param);
			return E_UNSPEC;
		}
		/* flags */
		for( ; p&&*p ; p++ )
		{
			switch (*p)
			{
				case 'g':
				case 'G':
					ap->ops|=AVPOPS_FLAG_ALL;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:fixup_delete_avp: bad flag "
						"<%c>\n",*p);
					return E_UNSPEC;
			}
		}
		/* force some flags: if no avp name is given, force "all" flag */
		if (ap->opd&AVPOPS_VAL_NONE)
			ap->ops |= AVPOPS_FLAG_ALL;

		pkg_free(*param);
		*param=(void*)ap;
	}

	return 0;
}


static int fixup_pushto_avp(void** param, int param_no)
{
	struct fis_param *ap;
	char *s;
	char *p;

	s = (char*)*param;
	ap = 0;

	if (param_no==1)
	{
		if ( *s!='$' || (++s)==0)
		{
			LOG(L_ERR,"ERROR:avops:fixup_pushto_avp: bad param 1; expected : "
				"$[ruri|hdr_name|..]\n");
			return E_UNSPEC;
		}
		/* compose the param structure */
		ap = (struct fis_param*)pkg_malloc(sizeof(struct fis_param));
		if (ap==0)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: no more pkg mem\n");
			return E_OUT_OF_MEM;
		}
		memset( ap, 0, sizeof(struct fis_param));

		if ( (p=strchr((char*)*param,'/'))!=0 )
			*(p++)=0;
		if (!strcasecmp( "ruri", s))
		{
			ap->opd = AVPOPS_VAL_NONE|AVPOPS_USE_RURI;
			if ( p && !(
				(!strcasecmp("username",p) && (ap->opd|=AVPOPS_FLAG_USER0)) ||
				(!strcasecmp("domain",p) && (ap->opd|=AVPOPS_FLAG_DOMAIN0)) ))
			{
				LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: unknown "
					" ruri flag \"%s\"!\n",p);
				return E_UNSPEC;
			}
		} else if (!strcasecmp( "duri", s)) {
			if ( p!=0 )
			{
				LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: unknown "
					" duri flag \"%s\"!\n",p);
				return E_UNSPEC;
			}
			ap->opd = AVPOPS_VAL_NONE|AVPOPS_USE_DURI;
		} else {
			/* what's the hdr destination ? request or reply? */
			if ( p==0 )
			{
				ap->opd = AVPOPS_USE_HDRREQ;
			} else {
				if (!strcasecmp( "request", p))
					ap->opd = AVPOPS_USE_HDRREQ;
				else if (!strcasecmp( "reply", p))
					ap->opd = AVPOPS_USE_HDRRPL;
				else
				{
					LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: header "
						"destination \"%s\" unknown!\n",p);
					return E_UNSPEC;
				}
			}
			/* copy header name */
			ap->val.s = (str*)pkg_malloc( sizeof(str) + strlen(s) + 1 );
			if (ap->val.s==0)
			{
				LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: no more pkg mem\n");
				return E_OUT_OF_MEM;
			}
			ap->val.s->s = ((char*)ap->val.s) + sizeof(str);
			ap->val.s->len = strlen(s);
			strcpy( ap->val.s->s, s);
		}
	} else if (param_no==2) {
		/* attribute name / alias */
		if ( (p=strchr(s,'/'))!=0 )
			*(p++)=0;
		if ( (ap=get_attr_or_alias(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: bad attribute name"
				"/alias <%s>\n", (char*)*param);
			return E_UNSPEC;
		}
		/* attr name is mandatory */
		if (ap->opd&AVPOPS_VAL_NONE)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: you must specify "
				"a name for the AVP\n");
			return E_UNSPEC;
		}
		/* flags */
		for( ; p&&*p ; p++ )
		{
			switch (*p) {
				case 'g':
				case 'G':
					ap->ops|=AVPOPS_FLAG_ALL;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:fixup_pushto_avp: bad flag "
						"<%c>\n",*p);
					return E_UNSPEC;
			}
		}
	}

	pkg_free(*param);
	*param=(void*)ap;
	return 0;
}



static int fixup_check_avp(void** param, int param_no)
{
	struct fis_param *ap;
	regex_t* re;
	char *s;

	s = (char*)*param;
	ap = 0;

	if (param_no==1)
	{
		if ( (ap=get_attr_or_alias(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_check_avp: bad attribute name"
				"/alias <%s>\n", (char*)*param);
			return E_UNSPEC;
		}
		/* attr name is mandatory */
		if (ap->opd&AVPOPS_VAL_NONE)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_check_avp: you must specify "
				"a name for the AVP\n");
			return E_UNSPEC;
		}
	} else if (param_no==2) {
		if ( (ap=parse_check_value(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_check_avp: failed to parse "
				"checked value \n");
			return E_UNSPEC;
		}
		/* if REGEXP op -> compile the expresion */
		if (ap->ops&AVPOPS_OP_RE)
		{
			if ( (ap->opd&AVPOPS_VAL_STR)==0 )
			{
				LOG(L_ERR,"ERROR:avpops:fixup_check_avp: regexp operation "
					"requires string value\n");
				return E_UNSPEC;
			}
			re = pkg_malloc(sizeof(regex_t));
			if (re==0)
			{
				LOG(L_ERR,"ERROR:avpops:fixup_check_avp: no more pkg mem\n");
				return E_OUT_OF_MEM;
			}
			DBG("DEBUG:avpops:fixup_check_avp: compiling regexp <%s>\n",
				ap->val.s->s);
			if (regcomp(re, ap->val.s->s, REG_EXTENDED|REG_ICASE|REG_NEWLINE))
			{
				pkg_free(re);
				LOG(L_ERR,"ERROR:avpops:fixip_check_avp: bad re <%s>\n",
					ap->val.s->s);
				return E_BAD_RE;
			}
			/* free the string and link the regexp */
			pkg_free(ap->val.s);
			ap->val.s = (str*)re;
		} else if (ap->ops&AVPOPS_OP_FM) {
			if ( !( ap->opd&AVPOPS_VAL_AVP ||
			(!(ap->opd&AVPOPS_VAL_AVP) && ap->opd&AVPOPS_VAL_STR) ) )
			{
				LOG(L_ERR,"ERROR:avpops:fixup_check_avp: fast_match operation "
					"requires string value or avp name/alias (%d/%d)\n",
					ap->opd, ap->ops);
				return E_UNSPEC;
			}
		}
	}

	pkg_free(*param);
	*param=(void*)ap;
	return 0;
}


static int fixup_copy_avp(void** param, int param_no)
{
	struct fis_param *ap;
	char *s;
	char *p;

	s = (char*)*param;
	ap = 0;
	p = 0;

	if (param_no==2)
	{
		/* avp / flags */
		if ( (p=strchr(s,'/'))!=0 )
			*(p++)=0;
	}

	if ( (ap=get_attr_or_alias(s))==0 )
	{
		LOG(L_ERR,"ERROR:avpops:fixup_copy_avp: bad attribute name"
			"/alias <%s>\n", (char*)*param);
		return E_UNSPEC;
	}
	/* attr name is mandatory */
	if (ap->opd&AVPOPS_VAL_NONE)
	{
		LOG(L_ERR,"ERROR:avpops:fixup_copy_avp: you must specify "
			"a name for the AVP\n");
		return E_UNSPEC;
	}

	if (param_no==2)
	{
		/* flags */
		for( ; p&&*p ; p++ )
		{
			switch (*p) {
				case 'g':
				case 'G':
					ap->ops|=AVPOPS_FLAG_ALL;
					break;
				case 'd':
				case 'D':
					ap->ops|=AVPOPS_FLAG_DELETE;
					break;
				case 'n':
				case 'N':
					ap->ops|=AVPOPS_FLAG_CASTN;
					break;
				case 's':
				case 'S':
					ap->ops|=AVPOPS_FLAG_CASTS;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:fixup_copy_avp: bad flag "
						"<%c>\n",*p);
					return E_UNSPEC;
			}
		}
	}

	pkg_free(*param);
	*param=(void*)ap;
	return 0;
}

static int fixup_printf(void** param, int param_no)
{
	struct fis_param *ap;
	xl_elem_t *model;

	if (param_no==1) {
		/* attribute name / alias */
		if ( (ap=get_attr_or_alias((char*)*param))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_printf: bad attribute name"
				"/alias <%s>\n", (char*)*param);
			return E_UNSPEC;
		}
		pkg_free(*param);
		*param=(void*)ap;
	} else if (param_no==2) {
		if(*param)
		{
			if(xl_parse_format((char*)(*param), &model, XL_DISABLE_COLORS)<0)
			{
				LOG(L_ERR, "ERROR:avpops:fixup_printf: wrong format[%s]\n",
					(char*)(*param));
				return E_UNSPEC;
			}
			
			*param = (void*)model;
			return 0;
		}
		else
		{
			LOG(L_ERR, "ERROR:avpops:fixup_printf: null format\n");
			return E_UNSPEC;
		}
	}

	return 0;
}

static int fixup_subst(void** param, int param_no)
{
	struct subst_expr* se;
	str subst;
	struct fis_param *ap;
	struct fis_param **av;
	char *s;
	char *p;
	
	if (param_no==1) {
		s = (char*)*param;
		ap = 0;
		p = 0;
		av = (struct fis_param**)pkg_malloc(2*sizeof(struct fis_param*));
		if(av==NULL)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_subst: no more memory\n");
			return E_UNSPEC;			
		}
		memset(av, 0, 2*sizeof(struct fis_param*));

		/* avp src / avp dst /flags */
		if ( (p=strchr(s,'/'))!=0 )
			*(p++)=0;
		if ( (ap=get_attr_or_alias(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_subst: bad attribute name"
				"/alias <%s>\n", (char*)*param);
			pkg_free(av);
			return E_UNSPEC;
		}
		/* attr name is mandatory */
		if (ap->opd&AVPOPS_VAL_NONE)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_subst: you must specify "
				"a name for the AVP\n");
			return E_UNSPEC;
		}
		av[0] = ap;
		if(p==0 || *p=='\0')
		{
			pkg_free(*param);
			*param=(void*)av;
			return 0;
		}
		
		/* dst */
		s = p;
		if ( (p=strchr(s,'/'))!=0 )
			*(p++)=0;
		if(p==0 || (p!=0 && p-s>1))
		{
			if ( (ap=get_attr_or_alias(s))==0 )
			{
				LOG(L_ERR,"ERROR:avpops:fixup_subst: bad attribute name"
					"/alias <%s>!\n", s);
				pkg_free(av);
				return E_UNSPEC;
			}
			/* attr name is mandatory */
			if (ap->opd&AVPOPS_VAL_NONE)
			{
				LOG(L_ERR,"ERROR:avpops:fixup_subst: you must specify "
					"a name for the AVP!\n");
				return E_UNSPEC;
			}
			av[1] = ap;
		}
		if(p==0 || *p=='\0')
		{
			pkg_free(*param);
			*param=(void*)av;
			return 0;
		}
		
		/* flags */
		for( ; p&&*p ; p++ )
		{
			switch (*p) {
				case 'g':
				case 'G':
					av[0]->ops|=AVPOPS_FLAG_ALL;
					break;
				case 'd':
				case 'D':
					av[0]->ops|=AVPOPS_FLAG_DELETE;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:fixup_subst: bad flag "
						"<%c>\n",*p);
					return E_UNSPEC;
			}
		}
		pkg_free(*param);
		*param=(void*)av;
	} else if (param_no==2) {
		DBG("%s:fixup_subst: fixing %s\n", exports.name, (char*)(*param));
		subst.s=*param;
		subst.len=strlen(*param);
		se=subst_parser(&subst);
		if (se==0){
			LOG(L_ERR, "ERROR:%s:fixup_subst: bad subst re %s\n",exports.name, 
					(char*)*param);
			return E_BAD_RE;
		}
		/* don't free string -- needed for specifiers */
		/* pkg_free(*param); */
		/* replace it with the compiled subst. re */
		*param=se;
	}

	return 0;
}

static int fixup_op_avp(void** param, int param_no)
{
	struct fis_param *ap;
	struct fis_param **av;
	char *s;
	char *p;

	s = (char*)*param;
	ap = 0;

	if (param_no==1)
	{
		av = (struct fis_param**)pkg_malloc(2*sizeof(struct fis_param*));
		if(av==NULL)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_op_avp: no more memory\n");
			return E_UNSPEC;			
		}
		memset(av, 0, 2*sizeof(struct fis_param*));
		/* avp src / avp dst */
		if ( (p=strchr(s,'/'))!=0 )
			*(p++)=0;

		if ( (av[0]=get_attr_or_alias(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_op_avp: bad attribute name"
				"/alias <%s>\n", (char*)*param);
			pkg_free(av);
			return E_UNSPEC;
		}
		/* attr name is mandatory */
		if (av[0]->opd&AVPOPS_VAL_NONE)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_op_avp: you must specify "
				"a name for the AVP\n");
			return E_UNSPEC;
		}
		if(p==0 || *p=='\0')
		{
			pkg_free(*param);
			*param=(void*)av;
			return 0;
		}
		
		s = p;
		if ( (ap=get_attr_or_alias(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_op_avp: bad attribute name"
				"/alias <%s>!\n", s);
			pkg_free(av);
			return E_UNSPEC;
		}
		/* attr name is mandatory */
		if (ap->opd&AVPOPS_VAL_NONE)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_op_avp: you must specify "
				"a name for the AVP!\n");
			return E_UNSPEC;
		}
		av[1] = ap;
		pkg_free(*param);
		*param=(void*)av;
		return 0;
	} else if (param_no==2) {
		if ( (ap=parse_op_value(s))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:fixup_op_avp: failed to parse "
				"the value \n");
			return E_UNSPEC;
		}
		/* only integer values or avps */
		if ( (ap->opd&AVPOPS_VAL_STR)!=0 && (ap->opd&AVPOPS_VAL_AVP)==0)
		{
			LOG(L_ERR,"ERROR:avpops:fixup_op_avp: operations "
				"requires integer values\n");
			return E_UNSPEC;
		}
		pkg_free(*param);
		*param=(void*)ap;
		return 0;
	}
	return -1;
}


static int fixup_is_avp_set(void** param, int param_no)
{
	struct fis_param *ap;
	char *p;

	if (param_no==1) {
		/* attribute name | alias / flags */
		if ( (p=strchr((char*)*param,'/'))!=0 )
			*(p++)=0;
		
		if ( ((ap=get_attr_or_alias((char*)*param))==0)
				|| (ap->opd&AVPOPS_VAL_NONE))
		{
			LOG(L_ERR,"ERROR:avpops:fixup_is_avp_set: bad attribute name"
				"/alias <%s>\n", (char*)*param);
			return E_UNSPEC;
		}
		if(p==0 || *p=='\0')
			ap->ops|=AVPOPS_FLAG_ALL;

		/* flags */
		for( ; p&&*p ; p++ )
		{
			switch (*p) {
				case 'n':
				case 'N':
					ap->ops|=AVPOPS_FLAG_CASTN;
					break;
				case 's':
				case 'S':
					ap->ops|=AVPOPS_FLAG_CASTS;
					break;
				default:
					LOG(L_ERR,"ERROR:avpops:fixup_is_avp_set: bad flag "
						"<%c>\n",*p);
					return E_UNSPEC;
			}
		}
		
		pkg_free(*param);
		*param=(void*)ap;
	}

	return 0;
}


static int w_dbload_avps(struct sip_msg* msg, char* source, char* param)
{
	return ops_dbload_avps ( msg, (struct fis_param*)source,
								(struct db_param*)param, use_domain);
}


static int w_dbstore_avps(struct sip_msg* msg, char* source, char* param)
{
	return ops_dbstore_avps ( msg, (struct fis_param*)source,
								(struct db_param*)param, use_domain);
}


static int w_dbdelete_avps(struct sip_msg* msg, char* source, char* param)
{
	return ops_dbdelete_avps ( msg, (struct fis_param*)source,
								(struct db_param*)param, use_domain);
}


static int w_write_avps(struct sip_msg* msg, char* source, char* param)
{
	return ops_write_avp ( msg, (struct fis_param*)source, 
								(struct fis_param*)param);
}


static int w_delete_avps(struct sip_msg* msg, char* param, char* foo)
{
	return ops_delete_avp ( msg, (struct fis_param*)param);
}


static int w_pushto_avps(struct sip_msg* msg, char* destination, char *param)
{
	return ops_pushto_avp ( msg, (struct fis_param*)destination,
								(struct fis_param*)param);
}


static int w_check_avps(struct sip_msg* msg, char* param, char *check)
{
	return ops_check_avp ( msg, (struct fis_param*)param,
								(struct fis_param*)check);
}

static int w_copy_avps(struct sip_msg* msg, char* name1, char *name2)
{
	return ops_copy_avp ( msg, (struct fis_param*)name1,
								(struct fis_param*)name2);
}

static int w_print_avps(struct sip_msg* msg, char* foo, char *bar)
{
	return ops_print_avp();
}

static int w_printf(struct sip_msg* msg, char* dest, char *format)
{
	return ops_printf(msg, (struct fis_param*)dest, (xl_elem_t*)format);
}

static int w_subst(struct sip_msg* msg, char* src, char *subst)
{
	return ops_subst(msg, (struct fis_param**)src, (struct subst_expr*)subst);
}

static int w_op_avps(struct sip_msg* msg, char* param, char *op)
{
	return ops_op_avp ( msg, (struct fis_param**)param,
								(struct fis_param*)op);
}

static int w_is_avp_set(struct sip_msg* msg, char* param, char *op)
{
	return ops_is_avp_set(msg, (struct fis_param*)param);
}


