/*
 * $Id: avpops_impl.c,v 1.14 2005/09/29 10:03:08 bogdan_iancu Exp $
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
 *  2005-01-30  "fm" (fast match) operator added (ramona)
 *  2005-01-30  avp_copy (copy/move operation) added (ramona)
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include <fnmatch.h>

#include "../../ut.h"
#include "../../dprint.h"
#include "../../usr_avp.h"
#include "../../action.h"
#include "../../ip_addr.h"
#include "../../config.h"
#include "../../dset.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../items.h"
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../mem/mem.h"
#include "avpops_impl.h"
#include "avpops_db.h"


static db_key_t  store_keys[6];
static db_val_t  store_vals[6];
static str      empty={"",0};


void init_store_avps( char **db_columns)
{
	/* unique user id */
	store_keys[0] = db_columns[0]; /*uuid*/
	store_vals[0].type = DB_STR;
	store_vals[0].nul  = 0;
	/* attribute */
	store_keys[1] = db_columns[1]; /*attribute*/
	store_vals[1].type = DB_STR;
	store_vals[1].nul  = 0;
	/* value */
	store_keys[2] = db_columns[2]; /*value*/
	store_vals[2].type = DB_STR;
	store_vals[2].nul  = 0;
	/* type */
	store_keys[3] = db_columns[3]; /*type*/
	store_vals[3].type = DB_INT;
	store_vals[3].nul  = 0;
	/* user name */
	store_keys[4] = db_columns[4]; /*username*/
	store_vals[4].type = DB_STR;
	store_vals[4].nul  = 0;
	/* domain */
	store_keys[5] = db_columns[5]; /*domain*/
	store_vals[5].type = DB_STR;
	store_vals[5].nul  = 0;
}


/* value 0 - attr value
 * value 1 - attr name
 * value 2 - attr type
 */
static int dbrow2avp(struct db_row *row, int flags, int_str attr,
														int just_val_flags)
{
	unsigned int uint;
	int  db_flags;
	str  atmp;
	str  vtmp;
	int_str avp_attr;
	int_str avp_val;

	if (just_val_flags==-1)
	{
		/* check for null fields into the row */
		if (row->values[0].nul || row->values[1].nul || row->values[2].nul )
		{
			LOG( L_ERR, "ERROR:avpops:dbrow2avp: dbrow contains NULL fields\n");
			return -1;
		}

		/* check the value types */
		if ( (row->values[0].type!=DB_STRING && row->values[0].type!=DB_STR)
			||  (row->values[1].type!=DB_STRING && row->values[1].type!=DB_STR)
			|| row->values[2].type!=DB_INT )
		{
			LOG(L_ERR,"ERROR:avpops:dbrow2avp: wrong field types in dbrow\n");
			return -1;
		}

		/* check the content of flag field */
		uint = (unsigned int)row->values[2].val.int_val;
		db_flags = ((uint&AVPOPS_DB_NAME_INT)?0:AVP_NAME_STR) |
			((uint&AVPOPS_DB_VAL_INT)?0:AVP_VAL_STR);
	
		DBG("db_flags=%d, flags=%d\n",db_flags,flags);
		/* does the avp type match ? */
		if(!((flags&(AVPOPS_VAL_INT|AVPOPS_VAL_STR))==0 ||
				((flags&AVPOPS_VAL_INT)&&((db_flags&AVP_NAME_STR)==0)) ||
				((flags&AVPOPS_VAL_STR)&&(db_flags&AVP_NAME_STR))))
			return -2;
	} else {
		/* check the validity of value column */
		if (row->values[0].nul || (row->values[0].type!=DB_STRING &&
		row->values[0].type!=DB_STR && row->values[0].type!=DB_INT) )
		{
			LOG(L_ERR,"ERROR:avpops:dbrow2avp: empty or wrong type for"
				" 'value' using scheme\n");
			return -1;
		}
		db_flags = just_val_flags;
	}

	/* is the avp name already known? */
	if ( (flags&AVPOPS_VAL_NONE)==0 )
	{
		/* use the name  */
		avp_attr = attr;
	} else {
		/* take the name from db response */
		if (row->values[1].type==DB_STRING)
		{
			atmp.s = (char*)row->values[1].val.string_val;
			atmp.len = strlen(atmp.s);
		} else {
			atmp = row->values[1].val.str_val;
		}
		if (db_flags&AVP_NAME_STR)
		{
			/* name is string */
			avp_attr.s = &atmp;
		} else {
			/* name is ID */
			if (str2int( &atmp, &uint)==-1)
			{
				LOG(L_ERR,"ERROR:avpops:dbrow2avp: name is not ID as "
					"flags say <%s>\n", atmp.s);
				return -1;
			}
			avp_attr.n = (int)uint;
		}
	}

	/* now get the value as correct type */
	if (row->values[0].type==DB_STRING)
	{
		vtmp.s = (char*)row->values[0].val.string_val;
		vtmp.len = strlen(vtmp.s);
	} else if (row->values[0].type==DB_STR){
		vtmp = row->values[0].val.str_val;
	}
	if (db_flags&AVP_VAL_STR) {
		/* value must be saved as string */
		if (row->values[0].type==DB_INT) {
			vtmp.s = int2str( (unsigned long)row->values[0].val.int_val,
				&vtmp.len);
		}
		avp_val.s = &vtmp;
	} else {
		/* value must be saved as integer */
		if (row->values[0].type!=DB_INT) {
			if (vtmp.len==0 || vtmp.s==0 || str2int(&vtmp, &uint)==-1) {
				LOG(L_ERR,"ERROR:avpops:dbrow2avp: value is not int "
					"as flags say <%s>\n", vtmp.s);
				return -1;
			}
			avp_val.n = (int)uint;
		} else {
			avp_val.n = row->values[0].val.int_val;
		}
	}

	/* added the avp */
	db_flags |= AVP_IS_IN_DB;
	return add_avp( (unsigned short)db_flags, avp_attr, avp_val);
}


inline static str* get_source_uri(struct sip_msg* msg,int source)
{
	/* which uri will be used? */
	if (source&AVPOPS_USE_FROM)
	{ /* from */
		if (parse_from_header( msg )<0 )
		{
			LOG(L_ERR,"ERROR:avpops:get_source_uri: failed "
				"to parse from\n");
			goto error;
		}
		return &(get_from(msg)->uri);
	} else if (source&AVPOPS_USE_TO)
	{  /* to */
		if (parse_headers( msg, HDR_TO_F, 0)<0)
		{
			LOG(L_ERR,"ERROR:avpops:get_source_uri: failed "
				"to parse to\n");
			goto error;
		}
		return &(get_to(msg)->uri);
	} else if (source&AVPOPS_USE_RURI) {  /* RURI */
		if(msg->new_uri.s!=NULL && msg->new_uri.len>0)
			return &(msg->new_uri);
		return &(msg->first_line.u.request.uri);
	} else {
		LOG(L_CRIT,"BUG:avpops:get_source_uri: unknow source <%d>\n",
			source);
		goto error;
	}
error:
	return 0;
}


static int parse_source_uri(struct sip_msg* msg,int source,struct sip_uri *uri)
{
	str *uri_s;

	/* get uri */
	if ( (uri_s=get_source_uri(msg,source))==0 )
	{
		LOG(L_ERR,"ERROR:avpops:parse_source_uri: cannot get uri\n");
		goto error;
	}

	/* parse uri */
	if (parse_uri(uri_s->s, uri_s->len , uri)<0)
	{
		LOG(L_ERR,"ERROR:avpops:parse_source_uri: failed to parse uri\n");
		goto error;
	}

	/* check uri */
	if (!uri->user.s || !uri->user.len || !uri->host.len || !uri->host.s )
	{
		LOG(L_ERR,"ERROR:avpops:parse_source_uri: incomplet uri <%.*s>\n",
			uri_s->len,uri_s->s);
		goto error;
	}

	return 0;
error:
	return -1;
}


static inline void int_str2db_val( int_str is_val, str *val, int is_s)
{
	if (is_s)
	{
		/* val is string */
		*val = *is_val.s;
	} else {
		/* val is integer */
		val->s =
			int2str((unsigned long)is_val.n, &val->len);
	}
}


static int get_avp_as_str(struct fis_param *ap ,str *val)
{
	struct usr_avp  *avp;
	unsigned short  name_type;
	int_str         avp_val;

	name_type = ((ap->opd&AVPOPS_VAL_INT)?0:AVP_NAME_STR);
	avp = search_first_avp( name_type, ap->val, &avp_val);
	if (avp==0)
	{
		DBG("DEBUG:avpops:get_val_as_str: no avp found\n");
		return -1;
	}
	int_str2db_val( avp_val, val, avp->flags&AVP_VAL_STR);
	return 0;
}


int ops_dbload_avps (struct sip_msg* msg, struct fis_param *sp,
									struct db_param *dbp, int use_domain)
{
	struct sip_uri   uri;
	db_res_t         *res;
	str              uuid;
	int  i, n, sh_flg;
	str *s0, *s1, *s2;

	s0 = s1 = s2 = NULL;
	if (sp->opd&AVPOPS_VAL_NONE)
	{
		/* get and parse uri */
		if(sp->opd&AVPOPS_FLAG_UUID0)
		{
			if((s0 = get_source_uri(msg,sp->opd))==NULL)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: failed to get uri!\n");
				goto error;
			}
		} else {
			if (parse_source_uri(msg, sp->opd, &uri)<0 )
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: failed to get uri\n");
				goto error;
			}
			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_USER0))
				s1 = &uri.user;
			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_DOMAIN0))
				s2 = &uri.host;
		}
		/* do DB query */
		res = db_load_avp( s0, s1,
				((use_domain)||(sp->opd&AVPOPS_FLAG_DOMAIN0))?s2:0,
				dbp->sa.s, dbp->table , dbp->scheme);
	} else if ((sp->opd&AVPOPS_VAL_AVP)||(sp->opd&AVPOPS_VAL_STR)) {
		/* get uuid from avp */
		if (sp->opd&AVPOPS_VAL_AVP)
		{
			if (get_avp_as_str(sp, &uuid)<0)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: failed to get uuid\n");
				goto error;
			}
		} else {
			uuid.s   = sp->val.s->s;
			uuid.len = sp->val.s->len;
		}
		
		if(sp->opd&AVPOPS_FLAG_UUID0)
		{
			s0 = &uuid;
		} else {
			/* parse uri */
			if (parse_uri(uuid.s, uuid.len, &uri)<0)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: failed to parse uri\n");
				goto error;
			}

			/* check uri */
			if(!uri.user.s|| !uri.user.len|| !uri.host.len|| !uri.host.s)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: incomplet uri <%.*s>\n",
					uuid.len, uuid.s);
				goto error;
			}

			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_USER0))
				s1 = &uri.user;
			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_DOMAIN0))
				s2 = &uri.host;
		}
		/* do DB query */
		res = db_load_avp( s0, s1,
				((use_domain)||(sp->opd&AVPOPS_FLAG_DOMAIN0))?s2:0,
				dbp->sa.s, dbp->table, dbp->scheme);
	} else {
		LOG(L_CRIT,"BUG:avpops:load_avps: invalid flag combination (%d/%d)\n",
			sp->opd, sp->ops);
		goto error;
	}

	/* res query ?  */
	if (res==0)
	{
		LOG(L_ERR,"ERROR:avpops:load_avps: db_load failed\n");
		goto error;
	}

	sh_flg = (dbp->scheme)?dbp->scheme->db_flags:-1;
	/* process the results */
	for( n=0,i=0 ; i<res->n ; i++)
	{
		/* validate row */
		if ( dbrow2avp( &res->rows[i], dbp->a.opd, dbp->a.val, sh_flg) < 0 )
			continue;
		n++;
	}

	db_close_query( res );

	DBG("DEBUG:avpops:load_avps: loaded avps = %d\n",n);

	return n?1:-1;
error:
	return -1;
}


int ops_dbstore_avps (struct sip_msg* msg, struct fis_param *sp,
									struct db_param *dbp, int use_domain)
{
	struct sip_uri   uri;
	struct usr_avp   **avp_list;
	struct usr_avp   *avp;
	unsigned short   name_type;
	int_str          i_s;
	str              uuid;
	int              keys_nr;
	int              n;
	str *s0, *s1, *s2;

	s0 = s1 = s2 = NULL;
	

	keys_nr = 6; /* uuid, avp name, avp val, avp type, user, domain */
	if (sp->opd&AVPOPS_VAL_NONE)
	{
		/* get and parse uri */
		if(sp->opd&AVPOPS_FLAG_UUID0)
		{
			if((s0 = get_source_uri(msg,sp->opd))==NULL)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: failed to get uri!\n");
				goto error;
			}
		} else {
			if (parse_source_uri(msg, sp->opd, &uri)<0 )
			{
				LOG(L_ERR,"ERROR:avpops:store_avps: failed to get uri\n");
				goto error;
			}
			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_USER0))
				s1 = &uri.user;
			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_DOMAIN0))
				s2 = &uri.host;
		}
		/* set values for keys  */
		store_vals[0].val.str_val = (s0)?*s0:empty;
		store_vals[4].val.str_val = (s1)?*s1:empty;
		if (use_domain || sp->opd&AVPOPS_FLAG_DOMAIN0)
			store_vals[5].val.str_val = (s2)?*s2:empty;
	} else if ((sp->opd&AVPOPS_VAL_AVP)||(sp->opd&AVPOPS_VAL_STR)) {
		/* get uuid from avp */
		if (sp->opd&AVPOPS_VAL_AVP)
		{
			if (get_avp_as_str(sp, &uuid)<0)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: failed to get uuid\n");
				goto error;
			}
		} else {
			uuid.s   = sp->val.s->s;
			uuid.len = sp->val.s->len;
		}
		
		if(sp->opd&AVPOPS_FLAG_UUID0)
		{
			s0 = &uuid;
		} else {
			/* parse uri */
			if (parse_uri(uuid.s, uuid.len, &uri)<0)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: failed to parse uri\n");
				goto error;
			}

			/* check uri */
			if(!uri.user.s|| !uri.user.len|| !uri.host.len|| !uri.host.s)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: incomplet uri <%.*s>\n",
					uuid.len, uuid.s);
				goto error;
			}

			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_USER0))
				s1 = &uri.user;
			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_DOMAIN0))
				s2 = &uri.host;
		}
		/* set values for keys  */
		store_vals[0].val.str_val = (s0)?*s0:empty;
		store_vals[4].val.str_val = (s1)?*s1:empty;
		if (use_domain || sp->opd&AVPOPS_FLAG_DOMAIN0)
			store_vals[5].val.str_val = (s2)?*s2:empty;
	} else {
		LOG(L_CRIT,"BUG:avpops:store_avps: invalid flag combination (%d/%d)\n",
			sp->opd, sp->ops);
		goto error;
	}

	/* set uuid/(username and domain) fields */
	n =0 ;

	if ((dbp->a.opd&AVPOPS_VAL_NONE)==0)
	{
		/* avp name is known ->set it and its type */
		name_type = (((dbp->a.opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
		store_vals[1].val.str_val = dbp->sa; /*attr name*/
		avp = search_first_avp( name_type, dbp->a.val, &i_s);
		for( ; avp; avp=search_next_avp(avp,&i_s))
		{
			/* don't insert avps which were loaded */
			if (avp->flags&AVP_IS_IN_DB)
				continue;
			/* set type */
			store_vals[3].val.int_val =
				(avp->flags&AVP_NAME_STR?0:AVPOPS_DB_NAME_INT)|
				(avp->flags&AVP_VAL_STR?0:AVPOPS_DB_VAL_INT);
			/* set value */
			int_str2db_val( i_s, &store_vals[2].val.str_val,
				avp->flags&AVP_VAL_STR);
			/* save avp */
			if (db_store_avp( store_keys, store_vals,
					keys_nr, dbp->table)==0 )
			{
				avp->flags |= AVP_IS_IN_DB;
				n++;
			}
		}
	} else {
		/* avp name is unknown -> go through all list */
		avp_list = get_avp_list();
		avp = *avp_list;

		for ( ; avp ; avp=avp->next )
		{
			/* don't insert avps which were loaded */
			if (avp->flags&AVP_IS_IN_DB)
				continue;
			/* check if type match */
			if ( !( (dbp->a.opd&(AVPOPS_VAL_INT|AVPOPS_VAL_STR))==0 ||
				((dbp->a.opd&AVPOPS_VAL_INT)&&((avp->flags&AVP_NAME_STR))==0)
				||((dbp->a.opd&AVPOPS_VAL_STR)&&(avp->flags&AVP_NAME_STR))))
				continue;

			/* set attribute name and type */
			if ( (i_s.s=get_avp_name(avp))==0 )
				i_s.n = avp->id;
			int_str2db_val( i_s, &store_vals[1].val.str_val,
				avp->flags&AVP_NAME_STR);
			store_vals[3].val.int_val =
				(avp->flags&AVP_NAME_STR?0:AVPOPS_DB_NAME_INT)|
				(avp->flags&AVP_VAL_STR?0:AVPOPS_DB_VAL_INT);
			/* set avp value */
			get_avp_val( avp, &i_s);
			int_str2db_val( i_s, &store_vals[2].val.str_val,
				avp->flags&AVP_VAL_STR);
			/* save avp */
			if (db_store_avp( store_keys, store_vals,
			keys_nr, dbp->table)==0)
			{
				avp->flags |= AVP_IS_IN_DB;
				n++;
			}
		}
	}

	DBG("DEBUG:avpops:store_avps: %d avps were stored\n",n);

	return n==0?-1:1;
error:
	return -1;
}



int ops_dbdelete_avps (struct sip_msg* msg, struct fis_param *sp,
									struct db_param *dbp, int use_domain)
{
	struct sip_uri  uri;
	int             res;
	str             uuid;
	str *s0, *s1, *s2;

	s0 = s1 = s2 = NULL;

	if (sp->opd&AVPOPS_VAL_NONE)
	{
		/* get and parse uri */
		if(sp->opd&AVPOPS_FLAG_UUID0)
		{
			if((s0 = get_source_uri(msg,sp->opd))==NULL)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: failed to get uri!\n");
				goto error;
			}
		} else {
			if (parse_source_uri( msg, sp->opd, &uri)<0 )
			{
				LOG(L_ERR,"ERROR:avpops:dbdelete_avps: failed to get uri\n");
				goto error;
			}
			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_USER0))
				s1 = &uri.user;
			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_DOMAIN0))
				s2 = &uri.host;
		}
		/* do DB delete */
		res = db_delete_avp(s0, s1,
				(use_domain||(sp->opd&AVPOPS_FLAG_DOMAIN0))?s2:0,
				dbp->sa.s, dbp->table);
	} else if ((sp->opd&AVPOPS_VAL_AVP)||(sp->opd&AVPOPS_VAL_STR)) {
		/* get uuid from avp */
		if (sp->opd&AVPOPS_VAL_AVP)
		{
			if (get_avp_as_str(sp, &uuid)<0)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: failed to get uuid\n");
				goto error;
			}
		} else {
			uuid.s   = sp->val.s->s;
			uuid.len = sp->val.s->len;
		}
		
		if(sp->opd&AVPOPS_FLAG_UUID0)
		{
			s0 = &uuid;
		} else {
			/* parse uri */
			if (parse_uri(uuid.s, uuid.len, &uri)<0)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: failed to parse uri\n");
				goto error;
			}

			/* check uri */
			if(!uri.user.s|| !uri.user.len|| !uri.host.len|| !uri.host.s)
			{
				LOG(L_ERR,"ERROR:avpops:load_avps: incomplet uri <%.*s>\n",
					uuid.len, uuid.s);
				goto error;
			}

			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_USER0))
				s1 = &uri.user;
			if((sp->opd&AVPOPS_FLAG_URI0)||(sp->opd&AVPOPS_FLAG_DOMAIN0))
				s2 = &uri.host;
		}
		/* do DB delete */
		res = db_delete_avp(s0, s1,
				(use_domain||(sp->opd&AVPOPS_FLAG_DOMAIN0))?s2:0,
				dbp->sa.s, dbp->table);
	} else {
		LOG(L_CRIT,
			"BUG:avpops:dbdelete_avps: invalid flag combination (%d/%d)\n",
			sp->opd, sp->ops);
		goto error;
	}

	/* res ?  */
	if (res<0)
	{
		LOG(L_ERR,"ERROR:avpops:dbdelete_avps: db_delete failed\n");
		goto error;
	}

	return 1;
error:
	return -1;
}


static inline str *search_hdr(struct sip_msg* msg, struct fis_param *hdr_def)
{
	static str body;
	struct hdr_field *hdr;

	/* parse all HDRs */
	if (parse_headers( msg, HDR_EOH_F, 0)!=0) {
		LOG(L_ERR,"ERROR:tm:append2buf: parsing hdrs failed\n");
		return 0;
	}
	/* search the HDR */
	if (hdr_def->opd&AVPOPS_VAL_INT) {
		for(hdr=msg->headers;hdr;hdr=hdr->next)
			if (hdr_def->val.n==hdr->type)
				goto found;
	} else {
		for(hdr=msg->headers;hdr;hdr=hdr->next)
			if (hdr_def->val.s->len==hdr->name.len &&
			strncasecmp( hdr_def->val.s->s, hdr->name.s, hdr->name.len)==0)
				goto found;
	}
	return 0;
found:
	/* trim the body */
	trim_len( body.len, body.s, hdr->body );
	return &body;
}


int ops_write_avp(struct sip_msg* msg, struct fis_param *src,
													struct fis_param *ap)
{
	struct sip_uri uri;
	int_str avp_val;
	unsigned short flags;
	str s_ip;

	if (src->opd&AVPOPS_VAL_NONE)
	{
		if (src->opd&AVPOPS_USE_SRC_IP)
		{
			/* get data from src_ip */
			if ( (s_ip.s=ip_addr2a( &msg->rcv.src_ip ))==0)
			{
				LOG(L_ERR,"ERROR:avpops:write_avp: cannot get src_ip\n");
				goto error;
			}
			s_ip.len = strlen(s_ip.s);
			avp_val.s = &s_ip;
		} else if (src->opd&AVPOPS_USE_DST_IP) {
			/* get data from dst ip */
			if ( (s_ip.s=ip_addr2a( &msg->rcv.dst_ip ))==0)
			{
				LOG(L_ERR,"ERROR:avpops:write_avp: cannot get dst_ip\n");
				goto error;
			}
			s_ip.len = strlen(s_ip.s);
			avp_val.s = &s_ip;
		} else if (src->opd&(AVPOPS_USE_HDRREQ|AVPOPS_USE_HDRRPL)) {
			/* get data from hdr */
			if ( (avp_val.s=search_hdr(msg,src))==0 ) {
				DBG("DEBUG:avpops:write_avp: hdr not found\n");
				goto error;
			}
		} else if (src->opd&AVPOPS_USE_DURI) {
			if(msg->dst_uri.s==NULL)
				goto error;
			avp_val.s = &msg->dst_uri;
		} else {
			/* get data from uri (from,to,ruri) */
			if (src->opd&(AVPOPS_FLAG_USER0|AVPOPS_FLAG_DOMAIN0))
			{
				if (parse_source_uri( msg, src->opd, &uri)!=0 )
				{
					LOG(L_ERR,"ERROR:avpops:write_avp: cannot parse uri\n");
					goto error;
				}
				if (src->opd&AVPOPS_FLAG_DOMAIN0)
					avp_val.s = &uri.host;
				else
					avp_val.s = &uri.user;
			} else {
				/* get whole uri */
				if ( (avp_val.s=get_source_uri(msg,src->opd))==0 )
				{
					LOG(L_ERR,"ERROR:avpops:write_avp: cannot get uri\n");
					goto error;
				}
			}
		}
		flags = AVP_VAL_STR;
	} else {
		avp_val = src->val;
		flags = (src->opd&AVPOPS_VAL_INT)?0:AVP_VAL_STR;
	}

	/* set the proper flag */
	flags |=  (ap->opd&AVPOPS_VAL_INT)?0:AVP_NAME_STR;

	/* added the avp */
	if (add_avp( flags, ap->val, avp_val)<0)
		goto error;

	return 1;
error:
	return -1;
}



int ops_delete_avp(struct sip_msg* msg, struct fis_param *ap)
{
	struct usr_avp **avp_list;
	struct usr_avp *avp;
	struct usr_avp *avp_next;
	unsigned short name_type;
	int n;

	n = 0;

	if ( (ap->opd&AVPOPS_VAL_NONE)==0)
	{
		/* avp name is known ->search by name */
		name_type = (((ap->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
		while ( (avp=search_first_avp( name_type, ap->val, 0))!=0 )
		{
			destroy_avp( avp );
			n++;
			if ( !(ap->ops&AVPOPS_FLAG_ALL) )
				break;
		}
	} else {
		/* avp name is not given - we have just flags */
		/* -> go through all list */
		avp_list = get_avp_list();
		avp = *avp_list;

		for ( ; avp ; avp=avp_next )
		{
			avp_next = avp->next;
			/* check if type match */
			if ( !( (ap->opd&(AVPOPS_VAL_INT|AVPOPS_VAL_STR))==0 ||
			((ap->opd&AVPOPS_VAL_INT)&&((avp->flags&AVP_NAME_STR))==0) ||
			((ap->opd&AVPOPS_VAL_STR)&&(avp->flags&AVP_NAME_STR)) )  )
				continue;
			/* remove avp */
			destroy_avp( avp );
			n++;
			if ( !(ap->ops&AVPOPS_FLAG_ALL) )
				break;
		}
	}

	DBG("DEBUG:avpops:remove_avps: %d avps were removed\n",n);

	return n?1:-1;
}



#define STR_BUF_SIZE  1024
static char str_buf[STR_BUF_SIZE];

inline static int compose_hdr(str *name, str *val, str *hdr, int new)
{
	char *p;
	char *s;
	int len;

	len = name->len+2+val->len+CRLF_LEN;
	if (new)
	{
		if ( (s=(char*)pkg_malloc(len))==0 )
		{
			LOG(L_ERR,"ERROR:avpops:compose_hdr: no more pkg mem\n");
			return -1;
		}
	} else {
		if ( len>STR_BUF_SIZE )
			return -1;
		s = str_buf;
	}
	p = s;
	memcpy(p, name->s, name->len);
	p += name->len;
	*(p++) = ':';
	*(p++) = ' ';
	memcpy(p, val->s, val->len);
	p += val->len;
	memcpy(p, CRLF, CRLF_LEN);
	p += CRLF_LEN;
	if (len!=p-s)
	{
		LOG(L_CRIT,"BUG:avpops:compose_hdr: buffer overflow\n");
		return -1;
	}
	hdr->len = len;
	hdr->s = s;
	return 0;
}



inline static int append_0(str *in, str *out)
{
	if (in->len+1>STR_BUF_SIZE)
		return -1;
	memcpy( str_buf, in->s, in->len);
	str_buf[in->len] = 0;
	out->len = in->len;
	out->s = str_buf;
	return 0;
}



int ops_pushto_avp ( struct sip_msg* msg, struct fis_param* dst,
													struct fis_param* ap)
{
	struct lump    *anchor;
	struct action  act;
	struct usr_avp *avp;
	unsigned short name_type;
	int_str        avp_val;
	str            val;
	int            act_type;
	int            n;

	/* search for the avp */
	name_type = (((ap->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
	avp = search_first_avp( name_type, ap->val, &avp_val);
	if (avp==0)
	{
		DBG("DEBUG:avpops:pushto_avp: no avp found\n");
		return -1;
	}

	n = 0;
	while (avp)
	{
		/* the avp val will be used all the time as str */
		if (avp->flags&AVP_VAL_STR) {
			val = *(avp_val.s);
		} else {
			val.s = int2str((unsigned long)avp_val.n, &val.len);
		}

		act_type = 0;
		/* push the value into right position */
		if (dst->opd&AVPOPS_USE_RURI)
		{
			if (dst->opd&AVPOPS_FLAG_USER0)
				act_type = SET_USER_T;
			else if (dst->opd&AVPOPS_FLAG_DOMAIN0)
				act_type = SET_HOST_T;
			else
				act_type = SET_URI_T;
			if ( avp->flags&AVP_VAL_STR && append_0( &val, &val)!=0 ) {
				LOG(L_ERR,"ERROR:avpops:pushto_avp: failed to make 0 term.\n");
				goto error;
			}
		} else if (dst->opd&(AVPOPS_USE_HDRREQ|AVPOPS_USE_HDRRPL)) {
			if ( compose_hdr( dst->val.s, &val, &val,
				dst->opd&AVPOPS_USE_HDRREQ)<0 )
			{
				LOG(L_ERR,"ERROR:avpops:pushto_avp: failed to build hdr\n");
				goto error;
			}
		} else if (dst->opd&AVPOPS_USE_DURI) {
			if (!(avp->flags&AVP_VAL_STR)) {
				goto error;
			}
		} else {
			LOG(L_CRIT,"BUG:avpops:pushto_avp: destination unknown (%d/%d)\n",
				dst->opd, dst->ops);
			goto error;
		}
	
		if ( act_type )
		{
			/* rewrite part of ruri */
			if (n)
			{
				/* if is not the first modification, push the current uri as
				 * branch */
				if (append_branch( msg, 0, 0, Q_UNSPECIFIED, 0, 0)!=1 )
				{
					LOG(L_ERR,"ERROR:avpops:pushto_avp: append_branch action"
						" failed\n");
					goto error;
				}
			}
			memset(&act, 0, sizeof(act));
			act.p1_type = STRING_ST;
			act.p1.string = val.s;
			act.type = act_type;
			if (do_action(&act, msg)<0)
			{
				LOG(L_ERR,"ERROR:avpops:pushto_avp: SET_XXXX_T action"
					" failed\n");
				goto error;
			}
		} else if (dst->opd&AVPOPS_USE_DURI) {
			if(set_dst_uri(msg, &val)!=0)
			{
				LOG(L_ERR,"ERROR:avpops:pushto_avp: changing dst uri failed\n");
				goto error;
			}
		} else if (dst->opd&AVPOPS_USE_HDRRPL) {
			/* set a header for reply */
			if (add_lump_rpl( msg , val.s, val.len, LUMP_RPL_HDR )==0)
			{
				LOG(L_ERR,"ERROR:avpops:pushto_avp: add_lump_rpl failed\n");
				goto error;
			}
		} else {
			/* set a header for request */
			if (parse_headers(msg, HDR_EOH_F, 0)==-1)
			{
				LOG(L_ERR, "ERROR:avpops:pushto_avp: message parse failed\n");
				goto error;
			}
			anchor = anchor_lump( msg, msg->unparsed-msg->buf, 0, 0);
			if (anchor==0)
			{
				LOG(L_ERR, "ERROR:avpops:pushto_avp: can't get anchor\n");
				goto error;
			}
			if (insert_new_lump_before(anchor, val.s, val.len, 0)==0)
			{
				LOG(L_ERR, "ERROR:avpops:pushto_avp: can't insert lump\n");
				goto error;
			}
		}

		n++;
		if ( !(ap->ops&AVPOPS_FLAG_ALL) )
			break;
		avp = search_next_avp( avp, &avp_val);
	} /* end while */

	DBG("DEBUG:avpops:pushto_avps: %d avps were processed\n",n);
	return 1;
error:
	return -1;
}


int ops_check_avp( struct sip_msg* msg, struct fis_param* ap,
													struct fis_param* val)
{
	unsigned short    name_type;
	struct usr_avp    *avp1;
	struct usr_avp    *avp2;
	regmatch_t        pmatch;
	int_str           avp_val;
	int_str           ck_val;
	str               s_ip;
	int               ck_flg;
	int               n;

	/* look if the required avp(s) is/are present */
	name_type = (((ap->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
	avp1 = search_first_avp( name_type, ap->val, &avp_val);
	if (avp1==0)
	{
		DBG("DEBUG:avpops:check_avp: no avp found to check\n");
		goto error;
	}

cycle1:
	if (val->opd&AVPOPS_VAL_AVP)
	{
		/* the 2nd operator is an avp name -> get avp val */
		name_type = (((val->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
		avp2 = search_first_avp( name_type, val->val, &ck_val);
		if (avp2==0)
		{
			DBG("DEBUG:avpops:check_avp: no avp2 found to check\n");
			goto error;
		}
		ck_flg = avp2->flags&AVP_VAL_STR?AVPOPS_VAL_STR:AVPOPS_VAL_INT;
	} else {
		ck_val = val->val;
		ck_flg = val->opd;
		avp2 = 0;
	}

cycle2:
	/* are both values of the same type? */
	if ( !( (avp1->flags&AVP_VAL_STR && ck_flg&AVPOPS_VAL_STR) ||
		((avp1->flags&AVP_VAL_STR)==0 && ck_flg&AVPOPS_VAL_INT) ||
		(avp1->flags&AVP_VAL_STR && ck_flg&AVPOPS_VAL_NONE)))
	{
		LOG(L_ERR,"ERROR:avpops:check_avp: value types don't match\n");
		goto next;
	}

	if (avp1->flags&AVP_VAL_STR)
	{
		/* string values to check */
		if (val->opd&AVPOPS_VAL_NONE)
		{
			/* value is variable */
			if (val->opd&AVPOPS_USE_SRC_IP)
			{
				/* get value from src_ip */
				if ( (s_ip.s=ip_addr2a( &msg->rcv.src_ip ))==0)
				{
					LOG(L_ERR,"ERROR:avpops:check_avp: cannot get src_ip\n");
					goto error;
				}
				s_ip.len = strlen(s_ip.s);
				ck_val.s = &s_ip;
			} else if (val->opd&AVPOPS_USE_DST_IP) {
				/* get value from dst ip */
				if ( (s_ip.s=ip_addr2a( &msg->rcv.dst_ip ))==0)
				{
					LOG(L_ERR,"ERROR:avpops:check_avp: cannot get dst_ip\n");
					goto error;
				}
				s_ip.len = strlen(s_ip.s);
				ck_val.s = &s_ip;
			} else {
				/* get value from uri */
				if ( (ck_val.s=get_source_uri(msg, val->opd))==0 )
				{
					LOG(L_ERR,"ERROR:avpops:check_avp: cannot get uri\n");
					goto next;
				}
			}
		}
		DBG("DEBUG:avpops:check_avp: check <%.*s> against <%.*s> as str\n",
			avp_val.s->len,avp_val.s->s,
			(val->ops&AVPOPS_OP_RE)?6:ck_val.s->len,
			(val->ops&AVPOPS_OP_RE)?"REGEXP":ck_val.s->s);
		/* do check */
		if (val->ops&AVPOPS_OP_EQ)
		{
			if (avp_val.s->len==ck_val.s->len)
			{
				if (val->ops&AVPOPS_FLAG_CI)
				{
					if (strncasecmp(avp_val.s->s,ck_val.s->s,ck_val.s->len)==0)
						return 1;
				} else {
					if (strncmp(avp_val.s->s,ck_val.s->s,ck_val.s->len)==0 )
						return 1;
				}
			}
		} else if (val->ops&AVPOPS_OP_NE) {
			if (avp_val.s->len==ck_val.s->len)
			{
				if (val->ops&AVPOPS_FLAG_CI)
				{
					if (strncasecmp(avp_val.s->s,ck_val.s->s,ck_val.s->len)!=0)
						return 1;
				} else {
					if (strncmp(avp_val.s->s,ck_val.s->s,ck_val.s->len)!=0 )
						return 1;
				}
			}
		} else if (val->ops&AVPOPS_OP_LT) {
			n = (avp_val.s->len>=ck_val.s->len)?avp_val.s->len:ck_val.s->len;
			if (strncasecmp(avp_val.s->s,ck_val.s->s,n)==-1)
				return 1;
		} else if (val->ops&AVPOPS_OP_LE) {
			n = (avp_val.s->len>=ck_val.s->len)?avp_val.s->len:ck_val.s->len;
			if (strncasecmp(avp_val.s->s,ck_val.s->s,n)<=0)
				return 1;
		} else if (val->ops&AVPOPS_OP_GT) {
			n = (avp_val.s->len>=ck_val.s->len)?avp_val.s->len:ck_val.s->len;
			if (strncasecmp(avp_val.s->s,ck_val.s->s,n)==1)
				return 1;
		} else if (val->ops&AVPOPS_OP_GE) {
			n = (avp_val.s->len>=ck_val.s->len)?avp_val.s->len:ck_val.s->len;
			if (strncasecmp(avp_val.s->s,ck_val.s->s,n)>=0)
				return 1;
		} else if (val->ops&AVPOPS_OP_RE) {
			if (regexec((regex_t*)ck_val.s, avp_val.s->s, 1, &pmatch, 0)==0)
				return 1;
		} else if (val->ops&AVPOPS_OP_FM){
			if (fnmatch( ck_val.s->s, avp_val.s->s, 0)==0)
				return 1;
		} else {
			LOG(L_CRIT,"BUG:avpops:check_avp: unknown operation "
				"(flg=%d/%d)\n",val->opd, val->ops);
		}
	} else {
		/* int values to check -> do check */
		DBG("DEBUG:avpops:check_avp: check <%d> against <%d> as int\n",
				avp_val.n, ck_val.n);
		if (val->ops&AVPOPS_OP_EQ)
		{
			if ( avp_val.n==ck_val.n)
				return 1;
		} else 	if (val->ops&AVPOPS_OP_NE) {
			if ( avp_val.n!=ck_val.n)
				return 1;
		} else  if (val->ops&AVPOPS_OP_LT) {
			if ( avp_val.n<ck_val.n)
				return 1;
		} else if (val->ops&AVPOPS_OP_LE) {
			if ( avp_val.n<=ck_val.n)
				return 1;
		} else if (val->ops&AVPOPS_OP_GT) {
			if ( avp_val.n>ck_val.n)
				return 1;
		} else if (val->ops&AVPOPS_OP_GE) {
			if ( avp_val.n>=ck_val.n)
				return 1;
		} else if (val->ops&AVPOPS_OP_BAND) {
			if ( avp_val.n&ck_val.n)
				return 1;
		} else if (val->ops&AVPOPS_OP_BOR) {
			if ( avp_val.n|ck_val.n)
				return 1;
		} else if (val->ops&AVPOPS_OP_BXOR) {
			if ( avp_val.n^ck_val.n)
				return 1;
		} else {
			LOG(L_CRIT,"BUG:avpops:check_avp: unknown operation "
				"(flg=%d)\n",val->ops);
		}
	}

next:
	/* cycle for the second value (only if avp can have multiple vals) */
	if ((val->opd&AVPOPS_VAL_AVP)&&(avp2=search_next_avp(avp2,&ck_val))!=0)
	{
		ck_flg = avp2->flags&AVP_VAL_STR?AVPOPS_VAL_STR:AVPOPS_VAL_INT;
		goto cycle2;
	/* cycle for the first value -> next avp */
	} else {
		avp1=(val->ops&AVPOPS_FLAG_ALL)?search_next_avp(avp1,&avp_val):0;
		if (avp1)
			goto cycle1;
	}

	DBG("DEBUG:avpops:check_avp: no match\n");
	return -1; /* check failed */
error:
	return -1;
}



int ops_copy_avp( struct sip_msg* msg, struct fis_param* name1,
													struct fis_param* name2)
{
	struct usr_avp *avp;
	struct usr_avp *prev_avp;
	int_str         avp_val;
	int_str         avp_val2;
	unsigned short name_type1;
	unsigned short name_type2;
	int n;
	str s;

	n = 0;
	prev_avp = 0;

	/* avp name is known ->search by name */
	name_type1 = (((name1->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
	name_type2 = (((name2->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);

	avp = search_first_avp( name_type1, name1->val, &avp_val);
	while ( avp )
	{
		/* build a new avp with new name, but old value */
		/* do we need cast conversion ?!?! */
		if((avp->flags&AVP_VAL_STR) && (name2->ops&AVPOPS_FLAG_CASTN)) {
			if(str2int(avp_val.s, (unsigned int*)&avp_val2.n)!=0)
			{
				LOG(L_ERR,"ERROR:avpops:copy_avp: cannot convert str to int\n");
				goto error;
			}
			if ( add_avp( name_type2, name2->val, avp_val2)==-1 ) {
				LOG(L_ERR,"ERROR:avpops:copy_avp: failed to create new avp!\n");
				goto error;
			}
		} else if(!(avp->flags&AVP_VAL_STR)&&(name2->ops&AVPOPS_FLAG_CASTS)) {
			s.s = int2str(avp_val.n, &s.len);
			avp_val2.s = &s;
			if ( add_avp( name_type2|AVP_VAL_STR, name2->val, avp_val2)==-1 ) {
				LOG(L_ERR,"ERROR:avpops:copy_avp: failed to create new avp.\n");
				goto error;
			}
		} else {
			if ( add_avp( name_type2|(avp->flags&AVP_VAL_STR), name2->val,
					avp_val)==-1 ) {
				LOG(L_ERR,"ERROR:avpops:copy_avp: failed to create new avp\n");
				goto error;
			}
		}
		n++;
		/* copy all avps? */
		if ( !(name2->ops&AVPOPS_FLAG_ALL) ) {
			/* delete the old one? */
			if (name2->ops&AVPOPS_FLAG_DELETE)
				destroy_avp( avp );
			break;
		} else {
			prev_avp = avp;
			avp = search_next_avp( prev_avp, &avp_val);
			/* delete the old one? */
			if (name2->ops&AVPOPS_FLAG_DELETE)
				destroy_avp( prev_avp );
		}
	}

	return n?1:-1;
error:
	return -1;
}



int ops_print_avp()
{
	struct usr_avp **avp_list;
	struct usr_avp *avp;
	int_str         val;
	str            *name;

	/* go through all list */
	avp_list = get_avp_list();
	avp = *avp_list;

	for ( ; avp ; avp=avp->next)
	{
		DBG("DEBUG:avpops:print_avp: p=%p, flags=%X\n",avp, avp->flags);
		if (avp->flags&AVP_NAME_STR)
		{
			name = get_avp_name(avp);
			DBG("DEBUG:\t\t\tname=<%.*s>\n",name->len,name->s);
		} else {
			DBG("DEBUG:\t\t\tid=<%d>\n",avp->id);
		}
		get_avp_val( avp, &val);
		if (avp->flags&AVP_VAL_STR)
		{
			DBG("DEBUG:\t\t\tval_str=<%.*s>\n",val.s->len,val.s->s);
		} else {
			DBG("DEBUG:\t\t\tval_int=<%d>\n",val.n);
		}
	}

	
	return 1;
}

#define AVP_PRINTBUF_SIZE 1024
int ops_printf(struct sip_msg* msg, struct fis_param* dest, xl_elem_t *format)
{
	static char printbuf[AVP_PRINTBUF_SIZE];
	int printbuf_len;
	int_str avp_val;
	unsigned short flags;
	str s;

	if(msg==NULL || dest==NULL || format==NULL)
	{
		LOG(L_ERR, "avpops:ops_printf: error - bad parameters\n");
		return -1;
	}
	
	printbuf_len = AVP_PRINTBUF_SIZE-1;
	if(xl_printf(msg, format, printbuf, &printbuf_len)<0)
	{
		LOG(L_ERR, "avpops:ops_printf: error - cannot print the format\n");
		return -1;
	}
	s.s   = printbuf;
	s.len = printbuf_len;
	avp_val.s = &s;
		
	/* set the proper flag */
	flags = AVP_VAL_STR;
	flags |=  (dest->opd&AVPOPS_VAL_INT)?0:AVP_NAME_STR;

	if (add_avp(flags, dest->val, avp_val)<0)
	{
		LOG(L_ERR, "avpops:ops_printf: error - cannot add AVP\n");
		return -1;
	}

	return 1;
}

int ops_subst(struct sip_msg* msg, struct fis_param** src,
		struct subst_expr* se)
{
	struct fis_param* newp;
	struct usr_avp *avp;
	struct usr_avp *prev_avp;
	int_str         avp_val;
	unsigned short name_type1;
	unsigned short name_type2;
	int n;
	int nmatches;
	str* result;

	n = 0;
	prev_avp = 0;

	/* avp name is known ->search by name */
	name_type1 = (((src[0]->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
	
	newp = (src[1]!=0)?src[1]:src[0];
	name_type2 = (((newp->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);

	avp = search_first_avp(name_type1, src[0]->val, &avp_val);
	while(avp)
	{
		if(!is_avp_str_val(avp))
		{
			prev_avp = avp;
			avp = search_next_avp(prev_avp, &avp_val);
			continue;
		}
		
		result=subst_str(avp_val.s->s, msg, se, &nmatches);
		if(result!=NULL)
		{
			/* build a new avp with new name */
			avp_val.s = result;
			if(add_avp(name_type2|AVP_VAL_STR, newp->val, avp_val)==-1 ) {
				LOG(L_ERR,"ERROR:avpops:ops_subst: failed to create new avp\n");
				if(result->s!=0)
					pkg_free(result->s);
				pkg_free(result);
				goto error;
			}
			if(result->s!=0)
				pkg_free(result->s);
			pkg_free(result);
			n++;
			/* copy all avps? */
			if (!(src[0]->ops&AVPOPS_FLAG_ALL) ) {
				/* delete the old one? */
				if (src[0]->ops&AVPOPS_FLAG_DELETE || src[1]==0)
					destroy_avp(avp);
				break;
			} else {
				prev_avp = avp;
				avp = search_next_avp(prev_avp, &avp_val);
				/* delete the old one? */
				if (src[0]->ops&AVPOPS_FLAG_DELETE || src[1]==0)
					destroy_avp( prev_avp );
			}
		} else {
			prev_avp = avp;
			avp = search_next_avp(prev_avp, &avp_val);
		}

	}
	DBG("avpops:ops_subst: subst to %d avps\n", n);
	return n?1:-1;
error:
	return -1;
}

int ops_op_avp( struct sip_msg* msg, struct fis_param** av,
													struct fis_param* val)
{
	unsigned short    name_type;
	unsigned short    name_type2;
	struct fis_param* ap;
	struct fis_param* newp;
	struct usr_avp    *avp1;
	struct usr_avp    *avp2;
	struct usr_avp    *prev_avp;
	int_str           avp_val;
	int_str           ck_val;
	int               ck_flg;
	int               result;

	ap = av[0];
	/* look if the required avp(s) is/are present */
	name_type = (((ap->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
	memset(&avp_val, 0, sizeof(int_str));
	avp1 = search_first_avp(name_type, ap->val, &avp_val);
	while(avp1!=0)
	{
		if(!(avp1->flags&AVP_VAL_STR))
			break;
		avp1 = search_next_avp(avp1, &avp_val);
	}
	if (avp1==0 && !(val->ops&AVPOPS_OP_BNOT)) {
		DBG("DEBUG:avpops:op_avp: no proper avp found\n");
		goto error;
	}
	
	newp = (av[1]!=0)?av[1]:av[0];
	name_type2 = (((newp->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
	prev_avp = 0;
	result = 0;

cycle1:
	if (val->opd&AVPOPS_VAL_AVP)
	{
		/* the 2nd operator is an avp name -> get avp val */
		name_type = (((val->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR);
		avp2 = search_first_avp(name_type, val->val, &ck_val);
		while(avp2!=0)
		{
			if(!(avp2->flags&AVP_VAL_STR))
				break;
			avp2 = search_next_avp(avp2, &ck_val);
		}
		if (avp2==0)
		{
			DBG("DEBUG:avpops:op_avp: no avp2 found\n");
			goto error;
		}
		ck_flg = AVPOPS_VAL_INT;
	} else {
		ck_val = val->val;
		ck_flg = val->opd;
		avp2 = 0;
	}

cycle2:
	/* do operation */
	DBG("DEBUG:avpops:op_avp: use <%d> and <%d>\n",
			avp_val.n, ck_val.n);
	if (val->ops&AVPOPS_OP_ADD)
	{
		result = avp_val.n+ck_val.n;
	} else 	if (val->ops&AVPOPS_OP_SUB) {
		result = avp_val.n-ck_val.n;
	} else  if (val->ops&AVPOPS_OP_MUL) {
		result = avp_val.n*ck_val.n;
	} else if (val->ops&AVPOPS_OP_DIV) {
		if(ck_val.n!=0)
			result = (int)(avp_val.n/ck_val.n);
		else
		{
			LOG(L_ERR, "avpops:op_avp: error - division by 0\n");
			result = 0;
		}
	} else if (val->ops&AVPOPS_OP_MOD) {
		if(ck_val.n!=0)
			result = avp_val.n%ck_val.n;
		else
		{
			LOG(L_ERR, "avpops:op_avp: error - modulo by 0\n");
			result = 0;
		}
	} else if (val->ops&AVPOPS_OP_BAND) {
		result = avp_val.n&ck_val.n;
	} else if (val->ops&AVPOPS_OP_BOR) {
		result = avp_val.n|ck_val.n;
	} else if (val->ops&AVPOPS_OP_BXOR) {
		result = avp_val.n^ck_val.n;
	} else if (val->ops&AVPOPS_OP_BNOT) {
		result = ~ck_val.n;
	} else {
		LOG(L_CRIT,"BUG:avpops:op_avp: unknown operation "
			"(flg=%d)\n",val->ops);
		goto error;
	}

	/* add the new avp */
	avp_val.n = result;
	if(add_avp(name_type2, newp->val, avp_val)==-1 ) {
		LOG(L_ERR,"ERROR:avpops:op_avp: failed to create new avp\n");
		goto error;
	}

	/* cycle for the second value (only if avp can have multiple vals) */
	while((val->opd&AVPOPS_VAL_AVP)&&(avp2=search_next_avp(avp2,&ck_val))!=0)
	{
		if(!(avp2->flags&AVP_VAL_STR))
		{
			ck_flg = AVPOPS_VAL_INT;
			goto cycle2;
		}
	}
	prev_avp = avp1;
	/* cycle for the first value -> next avp */
	while((val->ops&AVPOPS_FLAG_ALL)&&(avp1=search_next_avp(avp1,&avp_val))!=0)
	{
		if (!(avp1->flags&AVP_VAL_STR))
		{
			if(val->ops&AVPOPS_FLAG_DELETE && prev_avp!=0)
			{
				destroy_avp(prev_avp);
				prev_avp = 0;
			}
			goto cycle1;
		}
	}
	DBG("DEBUG:avpops:op_avp: done\n");
	if(val->ops&AVPOPS_FLAG_DELETE && prev_avp!=0)
	{
		destroy_avp(prev_avp);
		prev_avp = 0;
	}
	return 1;

error:
	return -1;
}

int ops_is_avp_set(struct sip_msg* msg, struct fis_param *ap)
{
	struct usr_avp *avp;
	
	avp=search_first_avp((((ap->opd&AVPOPS_VAL_INT))?0:AVP_NAME_STR),
				ap->val, 0);
	if(avp==0)
		return -1;
	if(ap->ops&AVPOPS_FLAG_ALL)
		return 1;
	
	do {
		if((ap->ops&AVPOPS_FLAG_CASTS && avp->flags&AVP_VAL_STR)
				|| (ap->ops&AVPOPS_FLAG_CASTN && !(avp->flags&AVP_VAL_STR)))
			return 1;
	} while ((avp=search_next_avp(avp, 0))!=0);
	
	return -1;
}


