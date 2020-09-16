/*
 * $Id: parse_cseq.h,v 1.2 2005/07/05 15:43:39 anomarme Exp $
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


#ifndef PARSE_CSEQ
#define PARSE_CSEQ

#include "../str.h"


struct cseq_body{
	int error;  /* Error code */
	str number; /* CSeq number */
	str method; /* Associated method */
	int method_id;  /* Associated method ID */
};


/* casting macro for accessing CSEQ body */
#define get_cseq(p_msg) ((struct cseq_body*)(p_msg)->cseq->parsed)


/*
 * Parse CSeq header field
 */
char* parse_cseq(char *buf, char* end, struct cseq_body* cb);


/*
 * Free all associated memory
 */
void free_cseq(struct cseq_body* cb);


#endif
