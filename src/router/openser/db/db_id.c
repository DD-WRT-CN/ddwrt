/* 
 * $Id: db_id.c,v 1.1 2005/06/16 12:07:08 miconda Exp $
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

#include "db_id.h"
#include "../dprint.h"
#include "../mem/mem.h"
#include "../ut.h"
#include <stdlib.h>
#include <string.h>


/*
 * Duplicate a string
 */
static int dupl_string(char** dst, const char* begin, const char* end)
{
	if (*dst) pkg_free(*dst);

	*dst = pkg_malloc(end - begin + 1);
	if ((*dst) == NULL) {
		return -1;
	}

	memcpy(*dst, begin, end - begin);
	(*dst)[end - begin] = '\0';
	return 0;
}


/*
 * Parse a database URL of form 
 * scheme://[username[:password]@]hostname[:port]/database
 *
 * Returns 0 if parsing was successful and -1 otherwise
 */
static int parse_db_url(struct db_id* id, const char* url)
{
#define SHORTEST_DB_URL "s://a/b"
#define SHORTEST_DB_URL_LEN (sizeof(SHORTEST_DB_URL) - 1)

	enum state {
		ST_SCHEME,     /* Scheme part */
		ST_SLASH1,     /* First slash */
		ST_SLASH2,     /* Second slash */
		ST_USER_HOST,  /* Username or hostname */
		ST_PASS_PORT,  /* Password or port part */
		ST_HOST,       /* Hostname part */
		ST_PORT,       /* Port part */
		ST_DB          /* Database part */
	};

	enum state st;
	int len, i;
	const char* begin;
	char* prev_token;

	prev_token = 0;

	if (!id || !url) {
		goto err;
	}
	
	len = strlen(url);
	if (len < SHORTEST_DB_URL_LEN) {
		goto err;
	}
	
	     /* Initialize all attributes to 0 */
	memset(id, 0, sizeof(struct db_id));
	st = ST_SCHEME;
	begin = url;

	for(i = 0; i < len; i++) {
		switch(st) {
		case ST_SCHEME:
			switch(url[i]) {
			case ':':
				st = ST_SLASH1;
				if (dupl_string(&id->scheme, begin, url + i) < 0) goto err;
				break;
			}
			break;

		case ST_SLASH1:
			switch(url[i]) {
			case '/':
				st = ST_SLASH2;
				break;

			default:
				goto err;
			}
			break;

		case ST_SLASH2:
			switch(url[i]) {
			case '/':
				st = ST_USER_HOST;
				begin = url + i + 1;
				break;
				
			default:
				goto err;
			}
			break;

		case ST_USER_HOST:
			switch(url[i]) {
			case '@':
				st = ST_HOST;
				if (dupl_string(&id->username, begin, url + i) < 0) goto err;
				begin = url + i + 1;
				break;

			case ':':
				st = ST_PASS_PORT;
				if (dupl_string(&prev_token, begin, url + i) < 0) goto err;
				begin = url + i + 1;
				break;

			case '/':
				if (dupl_string(&id->host, begin, url + i) < 0) goto err;
				if (dupl_string(&id->database, url + i + 1, url + len) < 0) goto err;
				return 0;
			}
			break;

		case ST_PASS_PORT:
			switch(url[i]) {
			case '@':
				st = ST_HOST;
				id->username = prev_token;
				if (dupl_string(&id->password, begin, url + i) < 0) goto err;
				begin = url + i + 1;
				break;

			case '/':
				id->host = prev_token;
				id->port = str2s(begin, url + i - begin, 0);
				if (dupl_string(&id->database, url + i + 1, url + len) < 0) goto err;
				return 0;
			}
			break;

		case ST_HOST:
			switch(url[i]) {
			case ':':
				st = ST_PORT;
				if (dupl_string(&id->host, begin, url + i) < 0) goto err;
				begin = url + i + 1;
				break;

			case '/':
				if (dupl_string(&id->host, begin, url + i) < 0) goto err;
				if (dupl_string(&id->database, url + i + 1, url + len) < 0) goto err;
				return 0;
			}
			break;

		case ST_PORT:
			switch(url[i]) {
			case '/':
				id->port = str2s(begin, url + i - begin, 0);
				if (dupl_string(&id->database, url + i + 1, url + len) < 0) goto err;
				return 0;
			}
			break;
			
		case ST_DB:
			break;
		}
	}

	if (st != ST_DB) goto err;
	return 0;

 err:
	if (id->scheme) pkg_free(id->scheme);
	if (id->username) pkg_free(id->username);
	if (id->password) pkg_free(id->password);
	if (id->host) pkg_free(id->host);
	if (id->database) pkg_free(id->database);
	if (prev_token) pkg_free(prev_token);
	return -1;
}


/*
 * Create a new connection identifier
 */
struct db_id* new_db_id(const char* url)
{
	struct db_id* ptr;

	if (!url) {
		LOG(L_ERR, "new_db_id: Invalid parameter\n");
		return 0;
	}

	ptr = (struct db_id*)pkg_malloc(sizeof(struct db_id));
	if (!ptr) {
		LOG(L_ERR, "new_db_id: No memory left\n");
		goto err;
	}
	memset(ptr, 0, sizeof(struct db_id));

	if (parse_db_url(ptr, url) < 0) {
		LOG(L_ERR, "new_db_id: Error while parsing database URL: %s\n", url);
		goto err;
	}

	return ptr;

 err:
	if (ptr) pkg_free(ptr);
	return 0;
}


/*
 * Compare two connection identifiers
 */
unsigned char cmp_db_id(struct db_id* id1, struct db_id* id2)
{
	if (!id1 || !id2) return 0;
	if (id1->port != id2->port) return 0;

	if (strcmp(id1->scheme, id2->scheme)) return 0;
	if (strcmp(id1->username, id2->username)) return 0;
	if (strcmp(id1->password, id2->password)) return 0;
	if (strcasecmp(id1->host, id2->host)) return 0;
	if (strcmp(id1->database, id2->database)) return 0;
	return 1;
}


/*
 * Free a connection identifier
 */
void free_db_id(struct db_id* id)
{
	if (!id) return;

	if (id->scheme) pkg_free(id->scheme);
	if (id->username) pkg_free(id->username);
	if (id->password) pkg_free(id->password);
	if (id->host) pkg_free(id->host);
	if (id->database) pkg_free(id->database);
	pkg_free(id);
}
