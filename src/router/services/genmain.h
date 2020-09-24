/*
 * genmain.h - helper header for generated main function
 *
 * Copyright (C) 2020 Sebastian Gottschall <gottschall@dd-wrt.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Id:
 */

#include <bcmnvram.h>
#include <shutils.h>
#include <utils.h>
#include <airbag.h>

static char *(*deps_func)(void);
static char *(*proc_func)(void);
static void (*start)(void);
static void (*stop)(void);
static int function;
static int force;
static void handle_procdeps(void)
{
	char *deps = NULL;
	int state = 1;
	if (deps_func) {
		deps = deps_func();
		dd_debug(DEBUG_SERVICE, "%s_deps exists, check nvram params %s\n", functiontable[function], deps);
		state = nvram_states(deps);
	}

	if (!state) {
		if (proc_func) {
			dd_debug(DEBUG_SERVICE, "%s_proc exists, check process\n", functiontable[function]);
			char *proc = proc_func();
			int pid = pidof(proc);
			dd_debug(DEBUG_SERVICE, "process name is %s, pid is %d\n", proc, pidof(proc));
			if (pid == -1)
				state = 1;
		}
	}
	if (force || state) {
		start();
		if (deps)
			nvram_states(deps);
	}

}

#define HANDLE_START(name, sym) \
	if (name == function) { \
		start_##sym(); \
		return 0; \
	}

#define HANDLE_START_DEPS(name, sym) \
	if (name == function) { \
		deps_func = sym##_deps; \
		start = start_##sym; \
		handle_procdeps(); \
		return 0; \
	}

#define HANDLE_START_PROC(name, sym) \
	if (name == function) { \
		proc_func = sym##_proc; \
		start = start_##sym; \
		handle_procdeps(); \
		return 0; \
	}

#define HANDLE_START_DEPS_PROC(name, sym) \
	if (name == function) { \
		deps_func = sym##_deps; \
		proc_func = sym##_proc; \
		start = start_##sym; \
		handle_procdeps(); \
		return 0; \
	}

#define HANDLE_STOP(name, sym) \
	if (name == function) { \
		stop_##sym(); \
		return 0; \
	}

#define HANDLE_RESTART(name, sym) \
	if (name == function) { \
		stop_##sym(); \
		start_##sym(); \
		return 0; \
	}

#define HANDLE_MAIN(name, sym) \
	if (name == function) { \
		return sym##_main(argc-2, args); \
	}

#include <stdlib.h>
static char **buildargs(int argc, char *argv[])
{
	char **args = (char **)malloc(sizeof(char **) * argc);
	int i;
	for (i = 0; i < argc - 3; i++)
		args[i + 1] = argv[i + 3];
	args[0] = argv[1];
	return args;
}

void end(char *argv[])
{
	dd_debug(DEBUG_SERVICE, "function %s_%s not found\n", argv[2], argv[1]);
}

int check_arguments(int argc, char *argv[])
{
	airbag_init();
	if (argc < 3) {
		fprintf(stdout, "%s servicename start|stop|restart|main args... [-f]\n", argv[0]);
		fprintf(stdout, "options:\n");
		fprintf(stdout, "-f : force start of service, no matter if neccessary\n");
		fprintf(stdout, "list of services:\n");
		int i;
		for (i = 0; i < sizeof(functiontable) / sizeof(struct fn); i++) {
			char feature[128] = { 0 };
			if (functiontable[i].start) {
				strcat(feature, "[start] ");
			}
			if (functiontable[i].stop) {
				strcat(feature, "[stop] ");
			}
			if (functiontable[i].stop && functiontable[i].start) {
				strcat(feature, "[restart] ");
			}
			if (functiontable[i].main) {
				strcat(feature, "[main]");
			}
			if (strlen(functiontable[i].name) > 15)
				fprintf(stdout, "\t%s\t%s\n", functiontable[i].name, feature);
			else if (strlen(functiontable[i].name) > 7)
				fprintf(stdout, "\t%s\t\t%s\n", functiontable[i].name, feature);
			else
				fprintf(stdout, "\t%s\t\t\t%s\n", functiontable[i].name, feature);
		}
		return -1;
	}
	int i;
	if (argc > 3 && !strcmp(argv[3], "-f"))
		force = 1;
	for (i = 0; i < sizeof(functiontable) / sizeof(struct fn); i++) {
		if (!strcmp(functiontable[i].name, argv[1])) {
			deps_func = functiontable[i].deps;
			proc_func = functiontable[i].proc;
			start = functiontable[i].start;
			stop = functiontable[i].stop;
			if (!strcmp(argv[2], "start") && start) {
				dd_debug(DEBUG_SERVICE, "call start for %s\n", argv[1]);
				if (deps_func || proc_func) {
					handle_procdeps();
				} else
					start();
				return 0;
			}
			if (!strcmp(argv[2], "stop") && stop) {
				dd_debug(DEBUG_SERVICE, "call stop for %s\n", argv[1]);
				stop();
				return 0;
			}
			if (!strcmp(argv[2], "restart") && stop && start) {
				dd_debug(DEBUG_SERVICE, "call restart for %s\n", argv[1]);
				stop();
				start();
				return 0;
			}
			if (!strcmp(argv[2], "main") && functiontable[i].main) {
				dd_debug(DEBUG_SERVICE, "call main for %s\n", argv[1]);
				char **args = buildargs(argc, argv);
				return functiontable[i].main(argc - 2, args);
			}
			end(argv);
			return -1;
		}
	}
	end(argv);
	fprintf(stderr, "method %s not found\n", argv[1]);
	return -1;
}
