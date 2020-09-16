/* 
 * $Id: db_pool.c,v 1.1 2005/06/16 12:07:08 miconda Exp $
 *
 * Copyright (C) 2001-2005 iptel.org
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

#include "../dprint.h"
#include "db_pool.h"


/* The head of the pool */
static struct pool_con* db_pool = 0;


/*
 * Search the pool for a connection with
 * the identifier equal to id, NULL is returned
 * when no connection is found
 */
struct pool_con* pool_get(struct db_id* id)
{
	struct pool_con* ptr;

	if (!id) {
		LOG(L_ERR, "pool_get: Invalid parameter value\n");
		return 0;
	}

	ptr = db_pool;
	while (ptr) {
		if (cmp_db_id(id, ptr->id)) {
			ptr->ref++;
			return ptr;
		}
		ptr = ptr->next;
	}

	return 0;
}


/*
 * Insert a new connection into the pool
 */
void pool_insert(struct pool_con* con)
{
	if (!con) return;

	con->next = db_pool;
	db_pool = con;
}


/*
 * Release connection from the pool, the function
 * would return 1 when if the connection is not
 * referenced anymore and thus can be closed and
 * deleted by the backend. The function returns
 * 0 if the connection should still be kept open
 * because some other module is still using it.
 * The function returns -1 if the connection is
 * not in the pool.
 */
int pool_remove(struct pool_con* con)
{
	struct pool_con* ptr;

	if (!con) return -2;

	if (con->ref > 1) {
		     /* There are still other users, just
		      * decrease the reference count and return
		      */
		DBG("pool_remove: Connection still kept in the pool\n");
		con->ref--;
		return 0;
	}

	DBG("pool_remove: Removing connection from the pool\n");

	if (db_pool == con) {
		db_pool = db_pool->next;
	} else {
		ptr = db_pool;
		while(ptr) {
			if (ptr->next == con) break;
			ptr = ptr->next;
		}
		if (!ptr) {
			LOG(L_ERR, "pool_remove: Weird, connection not found in the pool\n");
			return -1;
		} else {
			     /* Remove the connection from the pool */
			ptr->next = con->next;
		}
	}

	return 1;
}
