/*
 * $Id: pike_fifo.h,v 1.1.1.1 2005/06/13 16:47:43 bogdan_iancu Exp $
 *
 * PIKE module
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
 *  2004-05-12  created (bogdan)
 */




#ifndef _PIKE_FIFO_
#define _PIKE_FIFO_

#include <stdio.h>


#define PIKE_PRINT_IP_TREE  "print_ip_tree"
#define PIKE_PRINT_TIMER    "print_timer_list"

int fifo_print_ip_tree( FILE *fifo_stream, char *response_file );
int fifo_print_timer_list( FILE *fifo_stream, char *response_file );

#endif 
