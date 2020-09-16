/* 
 * $Id: flat_fifo.c,v 1.1.1.1 2005/06/13 16:47:38 bogdan_iancu Exp $ 
 *
 * Flatstore module FIFO interface
 *
 * Copyright (C) 2004 FhG Fokus
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

#include "../../dprint.h"
#include "../../fifo_server.h"
#include "flatstore_mod.h"
#include "flat_fifo.h"


#define FLAT_ROTATE "flat_rotate"
#define FLAT_ROTATE_LEN (sizeof(FLAT_ROTATE) - 1)


static int flat_rotate_cmd(FILE* pipe, char* response_file);


/*
 * Initialize the FIFO interface
 */
int init_flat_fifo(void)
{
	if (register_fifo_cmd(flat_rotate_cmd, FLAT_ROTATE, 0) < 0) {
		LOG(L_CRIT, "flatstore: Cannot register flat_rotate\n");
		return -1;
	}
	
	return 0;
}


static int flat_rotate_cmd(FILE* pipe, char* response_file)
{
	FILE* reply_file;
	
	reply_file = open_reply_pipe(response_file);
	if (reply_file == 0) {
		LOG(L_ERR, "flat_rotate_cmd: File not open\n");
		return -1;
	}

	*flat_rotate = time(0);
	fputs( "200 OK\n", reply_file);
	fclose(reply_file);
	return 1;
}
