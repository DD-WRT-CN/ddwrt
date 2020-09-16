/*
 * $Id: parse_methods.h,v 1.3 2005/07/05 15:43:39 anomarme Exp $
 *
 * Copyright (c) 2004 Juha Heinanen
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

#ifndef PARSE_METHODS_H
#define PARSE_METHODS_H

#include "../str.h"

/* 
 * Parse comma separated list of methods pointed by _body and assign their
 * enum bits to _methods.  Returns 1 on success and 0 on failure.
 */
char* parse_method(char* start, char* end, unsigned int* method);
int parse_methods(str* _body, unsigned int* _methods);


#endif /* PARSE_METHODS_H */
