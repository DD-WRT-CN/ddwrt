/* 
 * $Id: urirad_mod.c,v 1.4 2005/07/08 11:00:35 bogdan_iancu Exp $ 
 *
 * URI checks using Radius
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
 * 2003-03-15 - created by janakj
 * 2003-03-16 - flags export parameter added (janakj)
 */

#include <radiusclient-ng.h>

#include "../../dprint.h"
#include "../../config.h"
#include "../../sr_module.h"
#include "../acc/dict.h"
#include "urirad_mod.h"
#include "checks.h"

MODULE_VERSION

struct attr attrs[A_MAX];
struct val vals[V_MAX];
void *rh;

static int mod_init(void); /* Module initialization function */


/*
 * Module parameter variables
 */
static char* radius_config = DEFAULT_RADIUSCLEINT_CONF;
static int service_type = -1;

/*
 * Exported functions
 */
static cmd_export_t cmds[] = {
	{"radius_does_uri_exist", radius_does_uri_exist, 0, 0, REQUEST_ROUTE},
	{0, 0, 0, 0, 0}
};


/*
 * Exported parameters
 */
static param_export_t params[] = {
	{"radius_config", STR_PARAM, &radius_config},
	{"service_type", INT_PARAM, &service_type},
	{0, 0, 0}
};


/*
 * Module interface
 */
struct module_exports exports = {
	"uri_radius", 
	cmds,       /* Exported functions */
	params,     /* Exported parameters */
	mod_init,   /* module initialization function */
	0,          /* response function */
	0,          /* destroy function */
	0,          /* oncancel function */
	0           /* child initialization function */
};


static int mod_init(void)
{
	DBG("uri_radius - initializing\n");

	memset(attrs, 0, sizeof(attrs));
	memset(attrs, 0, sizeof(vals));
	attrs[A_SERVICE_TYPE].n	= "Service-Type";
	attrs[A_USER_NAME].n	= "User-Name";
	vals[V_CALL_CHECK].n	= "Call-Check";
	attrs[A_SIP_AVP].n		= "SIP-AVP";

	if ((rh = rc_read_config(radius_config)) == NULL) {
		LOG(L_ERR, "uri_radius: Error opening configuration file \n");
		return -1;
	}
    
	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0) {
		LOG(L_ERR, "uri_radius: Error opening dictionary file \n");
		return -2;
	}

	INIT_AV(rh, attrs, A_MAX, vals, "uri_radius", -3, -4);

	if (service_type != -1)
		vals[V_CALL_CHECK].v = service_type;

	return 0;
}
