/*
 * Copyright (C) 2012 STRATO.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include <sys/ioctl.h>
#include <unistd.h>

#include "ctree.h"
#include "ioctl.h"

#include "commands.h"
#include "utils.h"
#include "help.h"

static const char * const quota_cmd_group_usage[] = {
	"btrfs quota <command> [options] <path>",
	NULL
};

static int quota_ctl(int cmd, int argc, char **argv)
{
	int ret = 0;
	int fd;
	char *path = argv[1];
	struct btrfs_ioctl_quota_ctl_args args;
	DIR *dirstream = NULL;

	if (check_argc_exact(argc, 2))
		return -1;

	memset(&args, 0, sizeof(args));
	args.cmd = cmd;

	fd = btrfs_open_dir(path, &dirstream, 1);
	if (fd < 0)
		return 1;

	ret = ioctl(fd, BTRFS_IOC_QUOTA_CTL, &args);
	close_file_or_dir(fd, dirstream);
	if (ret < 0) {
		error("quota command failed: %m");
		return 1;
	}
	return 0;
}

static const char * const cmd_quota_enable_usage[] = {
	"btrfs quota enable <path>",
	"Enable subvolume quota support for a filesystem.",
	"Any data already present on the filesystem will not count towards",
	"the space usage numbers. It is recommended to enable quota for a",
	"filesystem before writing any data to it.",
	NULL
};

static int cmd_quota_enable(int argc, char **argv)
{
	int ret;

	clean_args_no_options(argc, argv, cmd_quota_enable_usage);

	ret = quota_ctl(BTRFS_QUOTA_CTL_ENABLE, argc, argv);

	if (ret < 0)
		usage(cmd_quota_enable_usage);
	return ret;
}

static const char * const cmd_quota_disable_usage[] = {
	"btrfs quota disable <path>",
	"Disable subvolume quota support for a filesystem.",
	NULL
};

static int cmd_quota_disable(int argc, char **argv)
{
	int ret;

	clean_args_no_options(argc, argv, cmd_quota_disable_usage);

	ret = quota_ctl(BTRFS_QUOTA_CTL_DISABLE, argc, argv);

	if (ret < 0)
		usage(cmd_quota_disable_usage);
	return ret;
}

static const char * const cmd_quota_rescan_usage[] = {
	"btrfs quota rescan [-sw] <path>",
	"Trash all qgroup numbers and scan the metadata again with the current config.",
	"",
	"-s   show status of a running rescan operation",
	"-w   wait for rescan operation to finish (can be already in progress)",
	NULL
};

static int cmd_quota_rescan(int argc, char **argv)
{
	int ret = 0;
	int fd;
	int e;
	char *path = NULL;
	struct btrfs_ioctl_quota_rescan_args args;
	unsigned long ioctlnum = BTRFS_IOC_QUOTA_RESCAN;
	DIR *dirstream = NULL;
	int wait_for_completion = 0;

	optind = 0;
	while (1) {
		int c = getopt(argc, argv, "sw");
		if (c < 0)
			break;
		switch (c) {
		case 's':
			ioctlnum = BTRFS_IOC_QUOTA_RESCAN_STATUS;
			break;
		case 'w':
			wait_for_completion = 1;
			break;
		default:
			usage(cmd_quota_rescan_usage);
		}
	}

	if (ioctlnum != BTRFS_IOC_QUOTA_RESCAN && wait_for_completion) {
		error("switch -w cannot be used with -s");
		return 1;
	}

	if (check_argc_exact(argc - optind, 1))
		usage(cmd_quota_rescan_usage);

	memset(&args, 0, sizeof(args));

	path = argv[optind];
	fd = btrfs_open_dir(path, &dirstream, 1);
	if (fd < 0)
		return 1;

	ret = ioctl(fd, ioctlnum, &args);
	e = errno;

	if (ioctlnum == BTRFS_IOC_QUOTA_RESCAN_STATUS) {
		close_file_or_dir(fd, dirstream);
		if (ret < 0) {
			error("could not obtain quota rescan status: %m");
			return 1;
		}
		if (!args.flags)
			printf("no rescan operation in progress\n");
		else
			printf("rescan operation running (current key %lld)\n",
				args.progress);
		return 0;
	}

	if (ret == 0) {
		printf("quota rescan started\n");
		fflush(stdout);
	} else if (ret < 0 && (!wait_for_completion || e != EINPROGRESS)) {
		error("quota rescan failed: %m");
		close_file_or_dir(fd, dirstream);
		return 1;
	}

	if (wait_for_completion) {
		ret = ioctl(fd, BTRFS_IOC_QUOTA_RESCAN_WAIT, &args);
		e = errno;
		if (ret < 0) {
			error("quota rescan wait failed: %m");
			close_file_or_dir(fd, dirstream);
			return 1;
		}
	}

	close_file_or_dir(fd, dirstream);
	return 0;
}

static const char quota_cmd_group_info[] =
"manage filesystem quota settings";

const struct cmd_group quota_cmd_group = {
	quota_cmd_group_usage, quota_cmd_group_info, {
		{ "enable", cmd_quota_enable, cmd_quota_enable_usage, NULL, 0 },
		{ "disable", cmd_quota_disable, cmd_quota_disable_usage,
		   NULL, 0 },
		{ "rescan", cmd_quota_rescan, cmd_quota_rescan_usage, NULL, 0 },
		NULL_CMD_STRUCT
	}
};

int cmd_quota(int argc, char **argv)
{
	return handle_command_group(&quota_cmd_group, argc, argv);
}
