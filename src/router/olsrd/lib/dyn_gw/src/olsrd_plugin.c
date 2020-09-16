/*
 * The olsr.org Optimized Link-State Routing daemon (olsrd)
 *
 * (c) by the OLSR project
 *
 * See our Git repository to find out who worked on this file
 * and thus is a copyright holder on it.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#include "olsrd_plugin.h"
#include "olsrd_dyn_gw.h"
#include "olsr.h"
#include "builddata.h"
#include "olsrd_dyn_gw.h"
#include <stdio.h>

#define PLUGIN_NAME              "OLSRD dyn_gw plugin"
#define PLUGIN_INTERFACE_VERSION 5

/**
 * "Private" declarations
 */

static void my_init(void) __attribute__ ((constructor));
static void my_fini(void) __attribute__ ((destructor));

/*
 * Defines the version of the plugin interface that is used
 * THIS IS NOT THE VERSION OF YOUR PLUGIN!
 * Do not alter unless you know what you are doing!
 */
int
olsrd_plugin_interface_version(void)
{
  return PLUGIN_INTERFACE_VERSION;
}

/**
 *Constructor
 */
void
my_init(void)
{
  /* Print plugin info to stdout */
  olsr_printf(0, "%s (%s)\n", PLUGIN_NAME, git_descriptor);
}

/**
 *Destructor
 */
void
my_fini(void)
{
  olsrd_plugin_fini();
}

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */
