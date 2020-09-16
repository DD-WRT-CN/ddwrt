/*
 * $Id: msfuncs.h,v 1.2 2005/08/26 14:08:45 miconda Exp $
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

#ifndef _MSFUNCS_H_
#define _MSFUNCS_H_

#include <time.h>
#include "../../str.h"

#define CT_TYPE		1
#define CT_CHARSET	2
#define CT_MSGR		4

#ifdef MSILO_TAG
#undef MSILO_TAG
#endif
#define MSILO_TAG	"msilo-HI4U-Ah0X-bZ98-"

typedef struct _content_type
{
	str type;
	str charset;
	str msgr;
} content_type_t;

/** apostrophes escape - useful for MySQL strings */
int m_apo_escape(char*, int, char*, int);

/** extract content-type value */
int m_extract_content_type(char*, int, content_type_t*, int);

/** build MESSAGE headers */
int m_build_headers(str *buf, str ctype, str contact);

/** build MESSAGE body */
int m_build_body(str *body, time_t date, str msg);

#endif

