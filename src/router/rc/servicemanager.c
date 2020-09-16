/* 
 * DD-WRT servicemanager.c
 *
 * Copyright (C) 2005 - 2019 Sebastian Gottschall <sebastian.gottschall@newmedia-net.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License
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
#include <stdio.h>
#include <shutils.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <bcmnvram.h>

#define SERVICE_MODULE "/lib/services.so"
static int *stops_running = NULL;

static void init_shared(void)
{
	if (!stops_running)
		stops_running = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

}

#define START 0x0
#define STOP 0x1
#define RESTART 0x2

static void _RELEASESTOPPED(const int method, const char *name)
{
	char fname[64];
	sprintf(fname, "/tmp/services/%s.%d", name, method);
	unlink(fname);
}

static int _STOPPED(const int method, const char *name)
{
	char fname[64];
	sprintf(fname, "/tmp/services/%s.%d", name, method);
	FILE *fp = fopen(fname, "rb");
	if (fp) {
		fclose(fp);
#if defined(HAVE_X86) || defined(HAVE_NEWPORT) || defined(HAVE_RB600) && !defined(HAVE_WDR4900)
		if (method == STOP) {
			if (stops_running)
				(*stops_running)--;
		}
#else
		dd_debug(DEBUG_SERVICE, "calling %s_%s not required!\n", method ? "stop" : "start", name);

		if (method == STOP) {
			if (stops_running)
				(*stops_running)--;
		}
#endif

		return 1;
	}
	fp = fopen(fname, "wb");
	if (fp) {
		fputs("s", fp);
		fclose(fp);
	}
	return 0;
}

static void _stopcondition(const int method, char *name)
{

}

#define STOPPED() if (_STOPPED(method, name)) { \
		     return 0; \
		    }

#define RELEASESTOPPED(a) _RELEASESTOPPED(a, name);

static int handle_service(const int method, const char *name, int force)
{
	int ret = 0;
	char *method_name = "start";
	if (method == STOP)
		method_name = "stop";
	if (method == RESTART)
		method_name = "restart";

	if (strcmp(name, "hotplug_block") && method != RESTART) {
		if (method == START)
			RELEASESTOPPED(STOP);
		if (method == STOP)
			RELEASESTOPPED(START);
		STOPPED();
	}
#if (!defined(HAVE_X86) && !defined(HAVE_NEWPORT) && !defined(HAVE_RB600)) || defined(HAVE_WDR4900)
	dd_debug(DEBUG_SERVICE, "%s:%s_%s", __func__, method_name, name);
#endif
	// lcdmessaged("Starting Service",name);
	char service[64];

	sprintf(service, "/etc/config/%s", name);
	FILE *ck = fopen(service, "rb");

	if (ck != NULL) {
		fclose(ck);
		return sysprintf("%s %s", service, method_name);
	}
	if (method == RESTART)
		return -1;
	char *args[] = { "/sbin/service", (char *)name, method_name, NULL };
	char *args_f[] = { "/sbin/service", (char *)name, method_name, "-f", NULL };

	if (force)
		ret = _evalpid(args_f, NULL, 0, NULL);
	else
		ret = _evalpid(args, NULL, 0, NULL);

	if (method == STOP) {
		if (stops_running)
			(*stops_running)--;
	}
#if (!defined(HAVE_X86) && !defined(HAVE_NEWPORT) && !defined(HAVE_RB600)) || defined(HAVE_WDR4900)
	if (stops_running)
		dd_debug(DEBUG_SERVICE, "calling done %s_%s (pending stops %d)\n", method_name, name, *stops_running);
	else
		dd_debug(DEBUG_SERVICE, "calling done %s_%s\n", method_name, name);
#endif
	return ret;
}

static void start_service_arg(char *name, int force)
{
	handle_service(START, name, force);
}

static void start_service(char *name)
{
	start_service_arg(name, 0);
}

static void start_service_force_arg(char *name, int force)
{
	RELEASESTOPPED(START);
	start_service_arg(name, force);
}

static void start_service_force(char *name)
{
	start_service_force_arg(name, 0);
}

static void start_service_f_arg(char *name, int force)
{
	FORK(start_service_arg(name, force));
}

static void start_service_f(char *name)
{
	start_service_f_arg(name, 0);
}

static void start_service_force_f_arg(char *name, int force)
{
	FORK(start_service_force_arg(name, force));
}

static void start_service_force_f(char *name)
{
	start_service_force_f_arg(name, 0);
}

static int start_main(char *name, int argc, char **argv)
{
	pid_t pid;
	int status;
	int sig;
	char *args[32] = { "/sbin/service", name, "main", NULL };
	int i;
	for (i = 1; i < argc && i < 30; i++)
		args[i + 2] = argv[i];
	args[2 + i] = NULL;
	switch (pid = fork()) {
	case -1:		/* error */
		perror("fork");
		return errno;
	case 0:		/* child */
		for (sig = 0; sig < (_NSIG - 1); sig++)
			signal(sig, SIG_DFL);
		execvp(args[0], args);
		perror(argv[0]);
		exit(errno);
	default:		/* parent */
		waitpid(pid, &status, 0);
		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		else
			return status;
	}
	return errno;
}

static void start_main_f(char *name, int argc, char **argv)
{
	FORK(start_main(name, argc, argv));
}

static int stop_running(void)
{
	return *stops_running > 0;
}

static int stop_running_main(int argc, char **argv)
{
	int dead = 0;
	while (stops_running != NULL && stop_running() && dead < 100) {
#if (!defined(HAVE_X86) && !defined(HAVE_NEWPORT) && !defined(HAVE_RB600)) || defined(HAVE_WDR4900)
		if (nvram_matchi("service_debugrunnings", 1))
			dd_loginfo("servicemanager", "%s: dead: %d running %d\n", __func__, dead, *stops_running);
#endif
		if (dead == 0)
			dd_loginfo("servicemanager", "waiting for services to finish (%d)...\n", *stops_running);
		usleep(100 * 1000);
		dead++;
	}
	if (dead == 50) {
		dd_logerror("servicemanager", "stopping processes taking too long!!!\n");
	} else if (stops_running != NULL && *stops_running == 0) {
		int *run = stops_running;
		stops_running = NULL;
		munmap(run, sizeof(int));
	}
	return 0;
}

static void stop_service(char *name)
{
	init_shared();
	if (stops_running)
		(*stops_running)++;
	handle_service(STOP, name, 0);
}

static void stop_service_force(char *name)
{
	RELEASESTOPPED(STOP);
	stop_service(name);
}

static void stop_service_f(char *name)
{
	init_shared();
	if (stops_running)
		(*stops_running)++;
	FORK(handle_service(STOP, name, 0));
}

static void stop_service_force_f(char *name)
{
	RELEASESTOPPED(STOP);
	init_shared();
	if (stops_running)
		(*stops_running)++;
	RELEASESTOPPED(STOP);
	FORK(handle_service(STOP, name, 0));
}

static void _restart_delay(char *name, int delay)
{
	if (delay)
		sleep(delay);
	if (handle_service(RESTART, name, 0)) {
		RELEASESTOPPED(STOP);
		RELEASESTOPPED(START);
		handle_service(STOP, name, 0);
		handle_service(START, name, 0);
	} else {
		if (stops_running)
			(*stops_running)--;
	}
}

static void restart(char *name)
{
	init_shared();
	if (stops_running)
		(*stops_running)++;
	_restart_delay(name, 0);
}

static void restart_fdelay(char *name, int delay)
{
	init_shared();
	if (stops_running)
		(*stops_running)++;
	FORK(_restart_delay(name, delay));
}

static void restart_f(char *name)
{
	restart_fdelay(name, 0);
}

static int restart_main(int argc, char **argv)
{
	restart(argv[1]);
	return 0;
}

static int restart_main_f(int argc, char **argv)
{
	char *name = argv[1];
	RELEASESTOPPED(STOP);
	RELEASESTOPPED(START);
	init_shared();
	if (stops_running)
		(*stops_running)++;
	FORK(_restart_delay(name, 0));
	return 0;
}
