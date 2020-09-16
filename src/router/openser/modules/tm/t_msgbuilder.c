/*
 * $Id: t_msgbuilder.c,v 1.7 2005/10/17 12:40:45 bogdan_iancu Exp $
 *
 * message printing
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
 *
 * History:
 * ----------
 * 2003-01-27  next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2003-02-13  build_uac_request uses proto (andrei)
 * 2003-02-28  scratchpad compatibility abandoned (jiri)
 * 2003-04-14  build_local no longer checks reply status as it
 *             is now called before reply status is updated to
 *             avoid late ACK sending (jiri)
 * 2003-10-02  added via_builder set host/port support (andrei)
 * 2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 * 2004-02-13: t->is_invite and t->local replaced with flags (bogdan)
 */

#include "../../hash_func.h"
#include "../../globals.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/parse_contact.h"
#include "t_msgbuilder.h"
#include "uac.h"


#define ROUTE_PREFIX "Route: "
#define ROUTE_PREFIX_LEN (sizeof(ROUTE_PREFIX) - 1)

#define ROUTE_SEPARATOR ", "
#define ROUTE_SEPARATOR_LEN (sizeof(ROUTE_SEPARATOR) - 1)

#define  append_string(_d,_s,_len) \
		do{\
			memcpy((_d),(_s),(_len));\
			(_d) += (_len);\
		}while(0);

#define LC(_cp) ((*(_cp))|0x20)
#define SET_FOUND(_new_state) \
	do{\
		fill->s=b;fill->len=p-b;\
		DBG("DEBUG:tm:extract_hdrs: hdr %d extracted as <%.*s>\n",\
			flag,fill->len,fill->s);\
		flags&=~(flag);\
		if (flags) {state=_new_state;}\
		else {goto done;}\
	}while(0)
#define GET_CSEQ() \
	do{\
		for(p++;p<end&&isspace((int)*p);p++);\
		for(fill->s=b;p<end&&isdigit((int)*p);p++);\
		fill->len=p-fill->s;\
		if ( (flags&=~(flag))==0) goto done;\
		state=1;\
	}while(0)
static int extract_hdrs( char *buf, int len, str *from, str *to, str *cseq)
{
	char *end, *p;
	char *b;
	str  *fill;
	int state;
	int flags;
	int flag;

	p = buf;
	end = buf+len;
	state = 1;
	b = 0;
	flags = ((from!=0)?0x1:0) | ((to!=0)?0x2:0) | ((cseq!=0)?0x4:0);
	flag = 0;
	fill = 0;

	while(p<end) {
		switch (*p) {
			case '\n':
			case '\r':
				switch (state) {
					case 4: state=5;break;
					case 5: case 6: state=6;break;
					default : state=2;break;
				}
				break;
			case ' ':
			case '\t':
				switch (state) {
					case 4: case 6: state=5; break;
					case 2: state=1; break;/*folded line*/
				}
				break;
			case ':':
				switch (state) {
					case 4:case 5: state=5;if(flag==0x04)GET_CSEQ();break;
					case 6: SET_FOUND(1);break;/*found*/
					case 2: state=1;break;
				}
				break;
			case 'f':
			case 'F':
				if (state==5) break;
				if (state==6) SET_FOUND(2);/*found*/;
				if (state!=2) {state = 1;break;}
				/* hdr starting with 'f' */
				if (from==0) break;
				b = p;
				if (p+3<end && LC(p+1)=='r' && LC(p+2)=='o' && LC(p+3)=='m')
					p+=3;
				state = 4; /* "f" or "from" found */
				fill = from;
				flag = 0x1;
				break;
			case 't':
			case 'T':
				if (state==5) break;
				if (state==6) SET_FOUND(2);/*found*/;
				if (state!=2) {state = 1;break;}
				/* hdr starting with 't' */
				if (to==0) break;
				b = p;
				if (p+1<end && LC(p+1)=='o')
					p+=1;
				state = 4; /* "t" or "to" found */
				fill = to;
				flag = 0x2;
				break;
			case 'c':
			case 'C':
				if (state==5) break;
				if (state==6) SET_FOUND(2);/*found*/;
				if (state!=2) {state = 1;break;}
				/* hdr starting with 'c' */
				if (cseq==0) break;
				if (p+3<end && LC(p+1)=='s' && LC(p+2)=='e' && LC(p+3)=='q') {
					b = p;
					p+=3;
					state = 4; /* "cseq" found */
					fill = cseq;
					flag = 0x4;
				}
				break;
			default:
				switch (state) {
					case 2:case 4: state=1; break;
					case 6: SET_FOUND(1);break;/*found*/;
				}
		}
		p++;
	}

	LOG(L_CRIT,"BUG:tm:extract_hdrs: no hdrs found in outgoing buffer\n");
	return -1;
done:
	return 0;
}


/* Build a local request based on a previous request; main
   customers of this function are local ACK and local CANCEL
 */
char *build_local(struct cell *Trans,unsigned int branch,
	unsigned int *len, char *method, int method_len, str *uas_to)
{
	char                *cancel_buf, *p, *via;
	unsigned int         via_len;
	struct hdr_field    *hdr;
	struct sip_msg      *req;
	char branch_buf[MAX_BRANCH_PARAM_LEN];
	str branch_str;
	struct hostport hp;
	str from;
	str to;
	str cseq_n;

	req = Trans->uas.request;
	from = Trans->from;
	cseq_n = Trans->cseq_n;
	to = *uas_to;

	if (req && req->msg_flags&(FL_USE_UAC_FROM|FL_USE_UAC_TO|FL_USE_UAC_CSEQ)) {
		if ( extract_hdrs( Trans->uac[branch].request.buffer,
		Trans->uac[branch].request.buffer_len,
		(req->msg_flags&FL_USE_UAC_FROM)?&from:0 ,
		(req->msg_flags&FL_USE_UAC_TO)?&to:0 ,
		(req->msg_flags&FL_USE_UAC_CSEQ)?&cseq_n:0 )!=0 ) {
			LOG(L_ERR, "ERROR:tm:build_local: "
				"failed to extract UAC hdrs\n");
			goto error;
		}
	}
	DBG("DEBUG:tm:build_local: using FROM=<%.*s>, TO=<%.*s>, CSEQ_N=<%.*s>\n",
		from.len,from.s , to.len,to.s , cseq_n.len,cseq_n.s);

	/* method, separators, version  */
	*len=SIP_VERSION_LEN + method_len + 2 /* spaces */ + CRLF_LEN;
	*len+=Trans->uac[branch].uri.len;

	/*via*/
	branch_str.s=branch_buf;
	if (!t_calc_branch(Trans,  branch, branch_str.s, &branch_str.len ))
		goto error;
	set_hostport(&hp, (is_local(Trans))?0:req);
	via=via_builder(&via_len, Trans->uac[branch].request.dst.send_sock,
		&branch_str, 0, Trans->uac[branch].request.dst.proto, &hp );
	if (!via)
	{
		LOG(L_ERR, "ERROR: build_local: "
			"no via header got from builder\n");
		goto error;
	}
	*len+= via_len;
	/*headers*/
	*len+=from.len+Trans->callid.len+to.len+cseq_n.len+1+method_len+CRLF_LEN;

	/* copy'n'paste Route headers */
	if (!is_local(Trans)) {
		for ( hdr=req->headers ; hdr ; hdr=hdr->next )
			 if (hdr->type==HDR_ROUTE_T)
				*len+=hdr->len;
	}

	/* User Agent */
	if (server_signature) {
		*len += USER_AGENT_LEN + CRLF_LEN;
	}
	/* Content Length, EoM */
	*len+=CONTENT_LENGTH_LEN+1 + CRLF_LEN + CRLF_LEN;

	cancel_buf=shm_malloc( *len+1 );
	if (!cancel_buf)
	{
		LOG(L_ERR, "ERROR: build_local: cannot allocate memory\n");
		goto error01;
	}
	p = cancel_buf;

	append_string( p, method, method_len );
	*(p++) = ' ';
	append_string( p, Trans->uac[branch].uri.s, Trans->uac[branch].uri.len);
	append_string( p, " " SIP_VERSION CRLF, 1+SIP_VERSION_LEN+CRLF_LEN );

	/* insert our via */
	append_string(p,via,via_len);

	/*other headers*/
	append_string( p, from.s, from.len );
	append_string( p, Trans->callid.s, Trans->callid.len );
	append_string( p, to.s, to.len );

	append_string( p, cseq_n.s, cseq_n.len );
	*(p++) = ' ';
	append_string( p, method, method_len );
	append_string( p, CRLF, CRLF_LEN );

	if (!is_local(Trans))  {
		for ( hdr=req->headers ; hdr ; hdr=hdr->next )
			if(hdr->type==HDR_ROUTE_T) {
				append_string(p, hdr->name.s, hdr->len );
			}
	}

	/* User Agent header */
	if (server_signature) {
		append_string(p,USER_AGENT CRLF, USER_AGENT_LEN+CRLF_LEN );
	}
	/* Content Length, EoM */
	append_string(p, CONTENT_LENGTH "0" CRLF CRLF ,
		CONTENT_LENGTH_LEN+1 + CRLF_LEN + CRLF_LEN);
	*p=0;

	pkg_free(via);
	return cancel_buf;
error01:
	pkg_free(via);
error:
	return NULL;
}


struct rte {
	rr_t* ptr;
	struct rte* next;
};


static inline void free_rte_list(struct rte* list)
{
	struct rte* ptr;
	
	while(list) {
		ptr = list;
		list = list->next;
		pkg_free(ptr);
	}
}


static inline int process_routeset(struct sip_msg* msg, str* contact, struct rte** list, str* ruri, str* next_hop)
{
	struct hdr_field* ptr;
	rr_t* p;
	struct rte* t, *head;
	struct sip_uri puri;
	
	ptr = msg->record_route;
	head = 0;
	while(ptr) {
		if (ptr->type == HDR_RECORDROUTE_T) {
			if (parse_rr(ptr) < 0) {
				LOG(L_ERR, "process_routeset: Error while parsing Record-Route header\n");
				return -1;
			}
			
			p = (rr_t*)ptr->parsed;
			while(p) {
				t = (struct rte*)pkg_malloc(sizeof(struct rte));
				if (!t) {
					LOG(L_ERR, "process_routeset: No memory left\n");
					free_rte_list(head);
					return -1;
				}
				t->ptr = p;
				t->next = head;
				head = t;
				p = p->next;
			}
		}
		ptr = ptr->next;
	}
	
	if (head) {
		if (parse_uri(head->ptr->nameaddr.uri.s, head->ptr->nameaddr.uri.len, &puri) == -1) {
			LOG(L_ERR, "process_routeset: Error while parsing URI\n");
			free_rte_list(head);
			return -1;
		}
		
		if (puri.lr.s) {
			     /* Next hop is loose router */
			*ruri = *contact;
			*next_hop = head->ptr->nameaddr.uri;
		} else {
			     /* Next hop is strict router */
			*ruri = head->ptr->nameaddr.uri;
			*next_hop = *ruri;
			t = head;
			head = head->next;
			pkg_free(t);
		}
	} else {
		     /* No routes */
		*ruri = *contact;
		*next_hop = *contact;
	}
	
	*list = head;
	return 0;
}


static inline int calc_routeset_len(struct rte* list, str* contact)
{
	struct rte* ptr;
	int ret;
	
	if (list || contact) {
		ret = ROUTE_PREFIX_LEN + CRLF_LEN;
	} else {
		return 0;
	}
	
	ptr = list;
	while(ptr) {
		if (ptr != list) {
			ret += ROUTE_SEPARATOR_LEN;
		}
		ret += ptr->ptr->len;
		ptr = ptr->next;
	}
	
	if (contact) {
		if (list) ret += ROUTE_SEPARATOR_LEN;
		ret += 2 + contact->len;
	}
	
	return ret;
}


     /*
      * Print the route set
      */
static inline char* print_rs(char* p, struct rte* list, str* contact)
{
	struct rte* ptr;
	
	if (list || contact) {
		memapp(p, ROUTE_PREFIX, ROUTE_PREFIX_LEN);
	} else {
		return p;
	}
	
	ptr = list;
	while(ptr) {
		if (ptr != list) {
			memapp(p, ROUTE_SEPARATOR, ROUTE_SEPARATOR_LEN);
		}
		
		memapp(p, ptr->ptr->nameaddr.name.s, ptr->ptr->len);
		ptr = ptr->next;
	}
	
	if (contact) {
		if (list) memapp(p, ROUTE_SEPARATOR, ROUTE_SEPARATOR_LEN);
		*p++ = '<';
		append_string(p, contact->s, contact->len);
		*p++ = '>';
	}
	
	memapp(p, CRLF, CRLF_LEN);
	return p;
}


     /*
      * Parse Contact header field body and extract URI
      * Does not parse headers !
      */
static inline int get_contact_uri(struct sip_msg* msg, str* uri)
{
	contact_t* c;
	
	uri->len = 0;
	if (!msg->contact) return 1;
	
	if (parse_contact(msg->contact) < 0) {
		LOG(L_ERR, "get_contact_uri: Error while parsing Contact body\n");
		return -1;
	}
	
	c = ((contact_body_t*)msg->contact->parsed)->contacts;
	
	if (!c) {
		LOG(L_ERR, "get_contact_uri: Empty body or * contact\n");
		return -2;
	}
	
	*uri = c->uri;
	return 0;
}



/*
 * The function creates an ACK to 200 OK. Route set will be created
 * and parsed. The function is used by tm when it generates
 * local ACK to 200 OK (on behalf of applications using uac
 */
char *build_dlg_ack(struct sip_msg* rpl, struct cell *Trans, unsigned int branch,
												str* to, unsigned int *len)
{
	char *req_buf, *p, *via;
	unsigned int via_len;
	char branch_buf[MAX_BRANCH_PARAM_LEN];
	int branch_len;
	str branch_str;
	struct hostport hp;
	struct rte* list;
	str contact, ruri, *cont;
	struct socket_info* send_sock;
	str next_hop;

	if (get_contact_uri(rpl, &contact) < 0) {
		return 0;
	}

	if (process_routeset(rpl, &contact, &list, &ruri, &next_hop) < 0) {
		return 0;
	}

	if ((contact.s != ruri.s) || (contact.len != ruri.len)) {
		     /* contact != ruri means that the next
		      * hop is a strict router, cont will be non-zero
		      * and print_routeset will append it at the end
		      * of the route set
		      */
		cont = &contact;
	} else {
		     /* Next hop is a loose router, nothing to append */
		cont = 0;
	}

	/* method, separators, version: "ACK sip:p2@iptel.org SIP/2.0" */
	*len = SIP_VERSION_LEN + ACK_LEN + 2 /* spaces */ + CRLF_LEN;
	*len += ruri.len;

	/* use same socket as for INVITE -bogdan */
	send_sock = Trans->uac[branch].request.dst.send_sock;

	if (!t_calc_branch(Trans,  branch, branch_buf, &branch_len)) goto error;
	branch_str.s = branch_buf;
	branch_str.len = branch_len;
	set_hostport(&hp, 0);

	/* build via */
	via = via_builder(&via_len, send_sock, &branch_str, 0, send_sock->proto, &hp);
	if (!via) {
		LOG(L_ERR, "build_dlg_ack: No via header got from builder\n");
		goto error;
	}
	*len+= via_len;
	
	     /*headers*/
	*len += Trans->from.len + Trans->callid.len + to->len + Trans->cseq_n.len + 1 + ACK_LEN + CRLF_LEN;
	
	     /* copy'n'paste Route headers */
	
	*len += calc_routeset_len(list, cont);
	
	     /* User Agent */
	if (server_signature) *len += USER_AGENT_LEN + CRLF_LEN;
	     /* Content Length, EoM */
	*len += CONTENT_LENGTH_LEN + 1 + CRLF_LEN + CRLF_LEN;
	
	req_buf = shm_malloc(*len + 1);
	if (!req_buf) {
		LOG(L_ERR, "build_dlg_ack: Cannot allocate memory\n");
		goto error01;
	}
	p = req_buf;

	append_string( p, ACK " ", ACK_LEN+1 );
	append_string(p, ruri.s, ruri.len );
	append_string( p, " " SIP_VERSION CRLF, 1 + SIP_VERSION_LEN + CRLF_LEN);

	/* insert our via */
	append_string(p, via, via_len);

	/*other headers*/
	append_string(p, Trans->from.s, Trans->from.len);
	append_string(p, Trans->callid.s, Trans->callid.len);
	append_string(p, to->s, to->len);

	append_string(p, Trans->cseq_n.s, Trans->cseq_n.len);
	*(p++) = ' ';
	append_string(p, ACK CRLF, ACK_LEN+CRLF_LEN);

	/* Routeset */
	p = print_rs(p, list, cont);

	/* User Agent header */
	if (server_signature) {
		append_string(p, USER_AGENT CRLF, USER_AGENT_LEN + CRLF_LEN);
	}

	/* Content Length, EoM */
	append_string(p, CONTENT_LENGTH "0" CRLF CRLF,
		CONTENT_LENGTH_LEN + 1 + CRLF_LEN + CRLF_LEN);
	*p = 0;

	pkg_free(via);
	free_rte_list(list);
	return req_buf;
	
 error01:
	pkg_free(via);
 error:
	free_rte_list(list);
	return 0;
  	 }


/*
 * Convert length of body into asciiz
 */
static inline int print_content_length(str* dest, str* body)
{
	static char content_length[10];
	int len;
	char* tmp;

	     /* Print Content-Length */
	if (body) {
		tmp = int2str(body->len, &len);
		if (len >= sizeof(content_length)) {
			LOG(L_ERR, "ERROR: print_content_length: content_len too big\n");
			return -1;
		}
		memcpy(content_length, tmp, len); 
		dest->s = content_length;
		dest->len = len;
	} else {
		dest->s = 0;
		dest->len = 0;
	}
	return 0;
}


/*
 * Convert CSeq number into asciiz
 */
static inline int print_cseq_num(str* _s, dlg_t* _d)
{
	static char cseq[INT2STR_MAX_LEN];
	char* tmp;
	int len;

	tmp = int2str(_d->loc_seq.value, &len);
	if (len > sizeof(cseq)) {
		LOG(L_ERR, "print_cseq_num: cseq too big\n");
		return -1;
	}
	
	memcpy(cseq, tmp, len);
	_s->s = cseq;
	_s->len = len;
	return 0;
}


/*
 * Create Via header
 */
static inline int assemble_via(str* dest, struct cell* t, struct socket_info* sock, int branch)
{
	static char branch_buf[MAX_BRANCH_PARAM_LEN];
	char* via;
	int len;
	unsigned int via_len;
	str branch_str;
	struct hostport hp;

	if (!t_calc_branch(t, branch, branch_buf, &len)) {
		LOG(L_ERR, "ERROR: assemble_via: branch calculation failed\n");
		return -1;
	}
	
	branch_str.s = branch_buf;
	branch_str.len = len;

#ifdef XL_DEBUG
	printf("!!!proto: %d\n", sock->proto);
#endif

	set_hostport(&hp, 0);
	via = via_builder(&via_len, sock, &branch_str, 0, sock->proto, &hp);
	if (!via) {
		LOG(L_ERR, "assemble_via: via building failed\n");
		return -2;
	}
	
	dest->s = via;
	dest->len = via_len;
	return 0;
}


/*
 * Print Request-URI
 */
static inline char* print_request_uri(char* w, str* method, dlg_t* dialog, struct cell* t, int branch)
{
	memapp(w, method->s, method->len); 
	memapp(w, " ", 1); 

	t->uac[branch].uri.s = w; 
	t->uac[branch].uri.len = dialog->hooks.request_uri->len;

	memapp(w, dialog->hooks.request_uri->s, dialog->hooks.request_uri->len); 
	memapp(w, " " SIP_VERSION CRLF, 1 + SIP_VERSION_LEN + CRLF_LEN);

	return w;
}


/*
 * Print To header field
 */
static inline char* print_to(char* w, dlg_t* dialog, struct cell* t)
{
	t->to.s = w;
	t->to.len = TO_LEN + dialog->rem_uri.len + CRLF_LEN;

	memapp(w, TO, TO_LEN);
	memapp(w, dialog->rem_uri.s, dialog->rem_uri.len);

	if (dialog->id.rem_tag.len) {
		t->to.len += TOTAG_LEN + dialog->id.rem_tag.len ;
		memapp(w, TOTAG, TOTAG_LEN);
		memapp(w, dialog->id.rem_tag.s, dialog->id.rem_tag.len);
	}

	memapp(w, CRLF, CRLF_LEN);
	return w;
}


/*
 * Print From header field
 */
static inline char* print_from(char* w, dlg_t* dialog, struct cell* t)
{
	t->from.s = w;
	t->from.len = FROM_LEN + dialog->loc_uri.len + CRLF_LEN;

	memapp(w, FROM, FROM_LEN);
	memapp(w, dialog->loc_uri.s, dialog->loc_uri.len);

	if (dialog->id.loc_tag.len) {
		t->from.len += FROMTAG_LEN + dialog->id.loc_tag.len;
		memapp(w, FROMTAG, FROMTAG_LEN);
		memapp(w, dialog->id.loc_tag.s, dialog->id.loc_tag.len);
	}

	memapp(w, CRLF, CRLF_LEN);
	return w;
}


/*
 * Print CSeq header field
 */
char* print_cseq_mini(char* target, str* cseq, str* method) {
	memapp(target, CSEQ, CSEQ_LEN);
	memapp(target, cseq->s, cseq->len);
	memapp(target, " ", 1);
	memapp(target, method->s, method->len);
	return target;
}

static inline char* print_cseq(char* w, str* cseq, str* method, struct cell* t)
{
	t->cseq_n.s = w; 
	/* don't include method name and CRLF -- subsequent
	 * local requests ACK/CANCEL will add their own */
	t->cseq_n.len = CSEQ_LEN + cseq->len; 
	w = print_cseq_mini(w, cseq, method);
	return w;
}

/*
 * Print Call-ID header field
 * created an extra function for pure header field creation, that is used by t_cancel for 
 * t_uac_cancel FIFO function.
 */
char* print_callid_mini(char* target, str callid) {
	memapp(target, CALLID, CALLID_LEN);
	memapp(target, callid.s, callid.len);
	memapp(target, CRLF, CRLF_LEN);
	return target;
}

static inline char* print_callid(char* w, dlg_t* dialog, struct cell* t)
{
	/* begins with CRLF, not included in t->callid, don`t know why...?!? */
	memapp(w, CRLF, CRLF_LEN);
	t->callid.s = w;
	t->callid.len = CALLID_LEN + dialog->id.call_id.len + CRLF_LEN;

	w = print_callid_mini(w, dialog->id.call_id);
	return w;
}


/*
 * Create a request
 */
char* build_uac_req(str* method, str* headers, str* body, dlg_t* dialog, int branch, 
			struct cell *t, int* len, struct socket_info* send_sock)
{
	char* buf, *w;
	str content_length, cseq, via;

	if (!method || !dialog) {
		LOG(L_ERR, "build_uac_req(): Invalid parameter value\n");
		return 0;
	}
	if (print_content_length(&content_length, body) < 0) {
		LOG(L_ERR, "build_uac_req(): Error while printing content-length\n");
		return 0;
	}
	if (print_cseq_num(&cseq, dialog) < 0) {
		LOG(L_ERR, "build_uac_req(): Error while printing CSeq number\n");
		return 0;
	}
	*len = method->len + 1 + dialog->hooks.request_uri->len + 1 + SIP_VERSION_LEN + CRLF_LEN;

	if (assemble_via(&via, t, send_sock, branch) < 0) {
		LOG(L_ERR, "build_uac_req(): Error while assembling Via\n");
		return 0;
	}
	*len += via.len;

	*len += TO_LEN + dialog->rem_uri.len
		+ (dialog->id.rem_tag.len ? (TOTAG_LEN + dialog->id.rem_tag.len) : 0) + CRLF_LEN;    /* To */
	*len += FROM_LEN + dialog->loc_uri.len
		+ (dialog->id.loc_tag.len ? (FROMTAG_LEN + dialog->id.loc_tag.len) : 0) + CRLF_LEN;  /* From */
	*len += CALLID_LEN + dialog->id.call_id.len + CRLF_LEN;                                      /* Call-ID */
	*len += CSEQ_LEN + cseq.len + 1 + method->len + CRLF_LEN;                                    /* CSeq */
	*len += calculate_routeset_length(dialog);                                                   /* Route set */
	*len += (body ? (CONTENT_LENGTH_LEN + content_length.len + CRLF_LEN) : 0);                   /* Content-Length */
	*len += (server_signature ? (USER_AGENT_LEN + CRLF_LEN) : 0);                                /* Signature */
	*len += (headers ? headers->len : 0);                                                        /* Additional headers */
	*len += (body ? body->len : 0);                                                              /* Message body */
	*len += CRLF_LEN;                                                                            /* End of Header */

	buf = shm_malloc(*len + 1);
	if (!buf) {
		LOG(L_ERR, "build_uac_req(): no shmem\n");
		goto error;
	}
	
	w = buf;

	w = print_request_uri(w, method, dialog, t, branch);  /* Request-URI */
	memapp(w, via.s, via.len);                            /* Top-most Via */
	w = print_to(w, dialog, t);                           /* To */
	w = print_from(w, dialog, t);                         /* From */
	w = print_cseq(w, &cseq, method, t);                  /* CSeq */
	w = print_callid(w, dialog, t);                       /* Call-ID */
	w = print_routeset(w, dialog);                        /* Route set */

	     /* Content-Length */
	if (body) {
		memapp(w, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		memapp(w, content_length.s, content_length.len);
		memapp(w, CRLF, CRLF_LEN);
	}
	
	     /* Server signature */
	if (server_signature) memapp(w, USER_AGENT CRLF, USER_AGENT_LEN + CRLF_LEN);
	if (headers) memapp(w, headers->s, headers->len);
	memapp(w, CRLF, CRLF_LEN);
     	if (body) memapp(w, body->s, body->len);

#ifdef EXTRA_DEBUG
	if (w-buf != *len ) abort();
#endif

	pkg_free(via.s);
	return buf;

 error:
	pkg_free(via.s);
	return 0;
}


int t_calc_branch(struct cell *t, 
	int b, char *branch, int *branch_len)
{
	return syn_branch ?
		branch_builder( t->hash_index,
			t->label, 0,
			b, branch, branch_len )
		: branch_builder( t->hash_index,
			0, t->md5,
			b, branch, branch_len );
}

