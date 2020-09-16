/*
 * $Id: parse_methods.c,v 1.5 2005/07/08 17:50:27 anomarme Exp $
 *
 * Copyright (c) 2004 Juha Heinanen
 *
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
 *  2005-07-05 - moved and merged method types in msg_parser.h (ramona)
 *             - changed and exported parse_method() to use it from other
 *               files (ramona)
 */

#include <strings.h>
#include "../dprint.h"
#include "../trim.h"
#include "parse_methods.h"
#include "msg_parser.h"


/*
 * Check if argument is valid RFC3261 token character.
 */
static inline int method_char(char _c)
{
 	return (_c >= 65 && _c <= 90)    /* upper alpha */
		|| (_c >= 97 && _c <= 122)   /* lower aplha */
 		|| (_c >= 48 && _c <= 57)    /* digits */
 		|| (_c == '-') || (_c == '.') || (_c == '%') || (_c == '*')
		|| (_c == '_') || (_c == '+') || (_c == '~') || (_c == '+');
 }
 
 
/*
 * Parse a method pointed by start, end is the last character to check (if NULL
 * assume that start is a zero terminated string)
 * => assign enum bit to method.
 * Returns pointer to next char if parse succeeded
 * and NULL otherwise.
 */
char* parse_method(char* start, char* end, unsigned int* method)
{
	int len=0;
	int max=0;
		
	 if (!start || !method) {
		 LOG(L_ERR, "parse_method: Invalid parameter value\n");
		 return NULL;
	 }

	 if(end)
		 max = end - start;
	 *method = METHOD_UNDEF;

	 switch (start[0]) {
		 case 'A':
		 case 'a':
			if(end && max<3)
				goto unknown;

			if ((start[1]=='c' || start[1]=='C')
					&& (start[2]=='k' || start[2]=='K'))
			{
				*method = METHOD_ACK;
				len = 3;
				goto done;
			}
			goto unknown;

		case 'B':
		case 'b':
			if(end && max<3)
				goto unknown;
			 
			if ((start[1]=='y' || start[1]=='Y')
					&& (start[2]=='e' || start[2]=='E'))
			{
				*method = METHOD_BYE;
				len = 3;
				goto done;
			}
			goto unknown;

	 	case 'C':
		case 'c':
			if(end && max<6)
				goto unknown;
			if ((start[1]=='a' || start[1]=='A')
					&& (start[2]=='n' || start[2]=='N')
					&& (start[3]=='c' || start[3]=='C')
					&& (start[4]=='e' || start[4]=='E')
					&& (start[5]=='l' || start[5]=='L'))
			{
				*method = METHOD_CANCEL;
				len = 6;
				goto done;
			}
			goto unknown;

	 	case 'I':
		case 'i':
			if(end && max<4)
				goto unknown;
			if(start[1]=='n' && start[1]=='N')
				goto unknown;
			
			if ((start[2]=='f' || start[2]=='F')
					&& (start[3]=='o' || start[3]=='O'))
			{
				*method = METHOD_INFO;
				len = 4;
				goto done;
			}
	
			if(end && max<6)
				goto unknown;
			if ((start[2]=='v' || start[2]=='V')
					&& (start[3]=='i' || start[3]=='I')
					&& (start[4]=='t' || start[4]=='T')
					&& (start[5]=='e' || start[5]=='E'))
			{
				*method = METHOD_INVITE;
				len = 6;
				goto done;
			}
			goto unknown;

	 	case 'M':
		case 'm':
			if(end && max<7)
				goto unknown;
			if ((start[1]=='e' || start[1]=='E')
					&& (start[2]=='s' || start[2]=='S')
					&& (start[3]=='s' || start[3]=='S')
					&& (start[4]=='a' || start[4]=='A')
					&& (start[5]=='g' || start[5]=='G')
					&& (start[6]=='e' || start[6]=='E')) {
				*method = METHOD_MESSAGE;
				len = 7;
				goto done;
			}
			goto unknown;

		case 'N':
		case 'n':
			if(end && max<6)
				goto unknown;
			if ((start[1]=='o' || start[1]=='O')
					&& (start[2]=='t' || start[2]=='T')
					&& (start[3]=='i' || start[3]=='I')
					&& (start[4]=='f' || start[4]=='F')
					&& (start[5]=='y' || start[5]=='Y'))
			{
				*method = METHOD_NOTIFY;
				len = 6;
				goto done;
			}
			goto unknown;

		case 'O':
		case 'o':
			if(end && max<7)
				goto unknown;
			if((start[1]=='p' || start[1]=='P')
					&& (start[2]=='t' || start[2]=='T')
					&& (start[3]=='i' || start[3]=='I')
					&& (start[4]=='o' || start[4]=='O')
					&& (start[5]=='n' || start[5]=='N')
					&& (start[6]=='s' || start[6]=='S'))
			{
				*method = METHOD_OPTIONS;
				len = 7;
				goto done;
			}
			goto unknown;

		case 'P':
		case 'p':
			if(end && max<5)
				goto unknown;
			if((start[1]=='r' || start[1]=='R')
					&& (start[2]=='a' || start[2]=='A')
					&& (start[3]=='c' || start[3]=='C')
					&& (start[4]=='k' || start[4]=='K'))
			{
				*method = METHOD_PRACK;
				len = 5;
				goto done;
			}
			goto unknown;

		case 'R':
		case 'r':
			if(end && max<5)
				goto unknown;
			if(start[1]=='e' && start[1]=='E')
				goto unknown;
			
 			if((start[2]=='f' || start[2]=='F')
					 && (start[3]=='e' || start[3]=='E')
					 && (start[4]=='R' || start[4]=='R'))
			{
 				*method = METHOD_REFER;
 				len = 5;
 				goto done;
 			}

			if(end && max<8)
				goto unknown;
			
			if ((start[2]=='g' || start[2]=='G')
					 && (start[3]=='i' || start[3]=='I')
					 && (start[4]=='s' || start[4]=='S')
					 && (start[5]=='t' || start[5]=='T')
					 && (start[6]=='e' || start[6]=='E')
					 && (start[7]=='r' || start[7]=='R'))
			{
				*method = METHOD_REGISTER;
				len = 8;
				goto done;
			}
			goto unknown;

	 	case 'S':
	 	case 's':
			if(end && max<9)
				goto unknown;
	 		if ((start[1]=='u' || start[1]=='U')
					 && (start[2]=='b' || start[2]=='B')
					 && (start[3]=='s' || start[3]=='S')
					 && (start[4]=='c' || start[4]=='C')
					 && (start[5]=='r' || start[5]=='R')
					 && (start[6]=='i' || start[6]=='I')
					 && (start[7]=='b' || start[7]=='B')
					 && (start[8]=='e' || start[8]=='E'))
			{
	 			*method = METHOD_SUBSCRIBE;
	 			len = 9;
				goto done;
			}
			goto unknown;

		case 'U':
		case 'u':
			if(end && max<6)
				goto unknown;
			if ((start[1]=='p' || start[1]=='P')
					&& (start[2]=='d' || start[2]=='D')
					&& (start[3]=='a' || start[3]=='A')
					&& (start[4]=='t' || start[4]=='T')
					&& (start[5]=='e' || start[5]=='E')) {
				*method = METHOD_UPDATE;
				len = 6;
				goto done;
			}
 			goto unknown;

		default:
			goto unknown;
		}
 
done:
	if(!end || (end && len < max))
	{
		if(start[len]!='\0' && start[len]!=',' && start[len]!=' '
				&& start[len]!='\t' && start[len]!='\r' && start[len]!='\n')
			goto unknown;
	}
	
	return (start+len);

unknown:
 	*method = METHOD_OTHER;
	if(end)
	{
		while(len < max)
		{
			if(!method_char(start[len]))
				return NULL;
			
			if((start[len]=='\0' || start[len]==',' || start[len]==' '
						|| start[len]=='\t' || start[len]=='\r'
						|| start[len]=='\n'))
				return (start+len);
			len++;
		}
		return end;
	}
	
	while(start[len]!='\0' && start[len]!=',' && start[len]!=' '
			&& start[len]!='\t' && start[len]!='\r' && start[len]!='\n')
	{
		if(!method_char(start[len]))
			return NULL;
		len++;
	}

	return (start+len);
}
 
 
/* 
 * Parse comma separated list of methods pointed by _body and assign their
 * enum bits to _methods.  Returns 0 on success and -1 on failure.
 */
int parse_methods(str* _body, unsigned int* _methods)
{
	str next;
	char *p;
	char *p0;
	unsigned int method;

	if (!_body || !_methods) {
		LOG(L_ERR, "parse_methods: Invalid parameter value\n");
		return -1;
	}

	next.len = _body->len;
	next.s = _body->s;

	trim_leading(&next);

	*_methods = 0;
	if (next.len == 0) {
		LOG(L_ERR, "ERROR: parse_methods: Empty body\n");
		return 0;
	}

	method = 0;
	p = next.s;
	
	while (p<next.s+next.len) {
		if((p0=parse_method(p, next.s+next.len, &method))!=NULL) {
			*_methods |= method;
			p = p0;
		} else {
			LOG(L_ERR, "ERROR: parse_methods: Invalid method\n");
			return -1;
		}
		
		while(p<next.s+next.len && (*p==' ' || *p=='\t'
					|| *p=='\r' || *p=='\n'))
			p++;
		if(p>=next.s+next.len || *p == '\0')
			return 0;
		
		
		if (*p == ',')
		{
			p++;
			while(p<next.s+next.len && (*p==' ' || *p=='\t'
					|| *p=='\r' || *p=='\n'))
				p++;
			if(p>=next.s+next.len)
				return 0;
		} else {
			LOG(L_ERR, "ERROR: parse_methods: Comma expected\n");
			return -1;
		}
	}

	return 0;
}
