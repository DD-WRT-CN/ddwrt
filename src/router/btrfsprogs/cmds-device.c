/*
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>
#include <getopt.h>

#include "kerncompat.h"
#include "ctree.h"
#include "ioctl.h"
#include "utils.h"
#include "volumes.h"
#include "cmds-fi-usage.h"

#include "commands.h"
#include "help.h"
#include "mkfs/common.h"

static const char * const device_cmd_group_usage[] = {
	"btrfs device <command> [<args>]",
	NULL
};

static const char * const cmd_device_add_usage[] = {
	"btrfs device add [options] <device> [<device>...] <path>",
	"Add a device to a filesystem",
	"-K|--nodiscard    do not perform whole device TRIM",
	"-f|--force        force overwrite existing filesystem on the disk",
	NULL
};

static int cmd_device_add(int argc, char **argv)
{
	char	*mntpnt;
	int i, fdmnt, ret = 0;
	DIR	*dirstream = NULL;
	int discard = 1;
	int force = 0;
	int last_dev;

	optind = 0;
	while (1) {
		int c;
		static const struct option long_options[] = {
			{ "nodiscard", optional_argument, NULL, 'K'},
			{ "force", no_argument, NULL, 'f'},
			{ NULL, 0, NULL, 0}
		};

		c = getopt_long(argc, argv, "Kf", long_options, NULL);
		if (c < 0)
			break;
		switch (c) {
		case 'K':
			discard = 0;
			break;
		case 'f':
			force = 1;
			break;
		default:
			usage(cmd_device_add_usage);
		}
	}

	if (check_argc_min(argc - optind, 2))
		usage(cmd_device_add_usage);

	last_dev = argc - 1;
	mntpnt = argv[last_dev];

	fdmnt = btrfs_open_dir(mntpnt, &dirstream, 1);
	if (fdmnt < 0)
		return 1;

	for (i = optind; i < last_dev; i++){
		struct btrfs_ioctl_vol_args ioctl_args;
		int	devfd, res;
		u64 dev_block_count = 0;
		char *path;

		res = test_dev_for_mkfs(argv[i], force);
		if (res) {
			ret++;
			continue;
		}

		devfd = open(argv[i], O_RDWR);
		if (devfd < 0) {
			error("unable to open device '%s'", argv[i]);
			ret++;
			continue;
		}

		res = btrfs_prepare_device(devfd, argv[i], &dev_block_count, 0,
				PREP_DEVICE_ZERO_END | PREP_DEVICE_VERBOSE |
				(discard ? PREP_DEVICE_DISCARD : 0));
		close(devfd);
		if (res) {
			ret++;
			goto error_out;
		}

		path = canonicalize_path(argv[i]);
		if (!path) {
			error("could not canonicalize pathname '%s': %m",
				argv[i]);
			ret++;
			goto error_out;
		}

		memset(&ioctl_args, 0, sizeof(ioctl_args));
		strncpy_null(ioctl_args.name, path);
		res = ioctl(fdmnt, BTRFS_IOC_ADD_DEV, &ioctl_args);
		if (res < 0) {
			error("error adding device '%s': %m", path);
			ret++;
		}
		free(path);
	}

error_out:
	close_file_or_dir(fdmnt, dirstream);
	return !!ret;
}

static int _cmd_device_remove(int argc, char **argv,
		const char * const *usagestr)
{
	char	*mntpnt;
	int i, fdmnt, ret = 0;
	DIR	*dirstream = NULL;

	clean_args_no_options(argc, argv, usagestr);

	if (check_argc_min(argc - optind, 2))
		usage(usagestr);

	mntpnt = argv[argc - 1];

	fdmnt = btrfs_open_dir(mntpnt, &dirstream, 1);
	if (fdmnt < 0)
		return 1;

	for(i = optind; i < argc - 1; i++) {
		struct	btrfs_ioctl_vol_args arg;
		struct btrfs_ioctl_vol_args_v2 argv2 = {0};
		int is_devid = 0;
		int	res;

		if (string_is_numerical(argv[i])) {
			argv2.devid = arg_strtou64(argv[i]);
			argv2.flags = BTRFS_DEVICE_SPEC_BY_ID;
			is_devid = 1;
		} else if (is_block_device(argv[i]) == 1 ||
				strcmp(argv[i], "missing") == 0) {
			strncpy_null(argv2.name, argv[i]);
		} else {
			error("not a block device: %s", argv[i]);
			ret++;
			continue;
		}

		/*
		 * Positive values are from BTRFS_ERROR_DEV_*,
		 * otherwise it's a generic error, one of errnos
		 */
		res = ioctl(fdmnt, BTRFS_IOC_RM_DEV_V2, &argv2);

		/*
		 * If BTRFS_IOC_RM_DEV_V2 is not supported we get ENOTTY and if
		 * argv2.flags includes a flag which kernel doesn't understand then
		 * we shall get EOPNOTSUPP
		 */
		if (res < 0 && (errno == ENOTTY || errno == EOPNOTSUPP)) {
			if (is_devid) {
				error("device delete by id failed: %m");
				ret++;
				continue;
			}
			memset(&arg, 0, sizeof(arg));
			strncpy_null(arg.name, argv[i]);
			res = ioctl(fdmnt, BTRFS_IOC_RM_DEV, &arg);
		}

		if (res) {
			const char *msg;

			if (res > 0)
				msg = btrfs_err_str(res);
			else
				msg = strerror(errno);
			if (is_devid) {
				error("error removing devid %llu: %s",
					(unsigned long long)argv2.devid, msg);
			} else {
				error("error removing device '%s': %s",
					argv[i], msg);
			}
			ret++;
		}
	}

	close_file_or_dir(fdmnt, dirstream);
	return !!ret;
}

#define COMMON_USAGE_REMOVE_DELETE					\
	"If 'missing' is specified for <device>, the first device that is",	\
	"described by the filesystem metadata, but not present at the mount",	\
	"time will be removed. (only in degraded mode)"

static const char * const cmd_device_remove_usage[] = {
	"btrfs device remove <device>|<devid> [<device>|<devid>...] <path>",
	"Remove a device from a filesystem",
	"",
	COMMON_USAGE_REMOVE_DELETE,
	NULL
};

static int cmd_device_remove(int argc, char **argv)
{
	return _cmd_device_remove(argc, argv, cmd_device_remove_usage);
}

static const char * const cmd_device_delete_usage[] = {
	"btrfs device delete <device>|<devid> [<device>|<devid>...] <path>",
	"Remove a device from a filesystem (alias of \"btrfs device remove\")",
	"",
	COMMON_USAGE_REMOVE_DELETE,
	NULL
};

static int cmd_device_delete(int argc, char **argv)
{
	return _cmd_device_remove(argc, argv, cmd_device_delete_usage);
}

static const char * const cmd_device_scan_usage[] = {
	"btrfs device scan [(-d|--all-devices)|<device> [<device>...]]",
	"Scan devices for a btrfs filesystem",
	" -d|--all-devices (deprecated)",
	NULL
};

static int cmd_device_scan(int argc, char **argv)
{
	int i;
	int devstart;
	int all = 0;
	int ret = 0;

	optind = 0;
	while (1) {
		int c;
		static const struct option long_options[] = {
			{ "all-devices", no_argument, NULL, 'd'},
			{ NULL, 0, NULL, 0}
		};

		c = getopt_long(argc, argv, "d", long_options, NULL);
		if (c < 0)
			break;
		switch (c) {
		case 'd':
			all = 1;
			break;
		default:
			usage(cmd_device_scan_usage);
		}
	}
	devstart = optind;

	if (all && check_argc_max(argc - optind, 1))
		usage(cmd_device_scan_usage);

	if (all || argc - optind == 0) {
		printf("Scanning for Btrfs filesystems\n");
		ret = btrfs_scan_devices();
		error_on(ret, "error %d while scanning", ret);
		ret = btrfs_register_all_devices();
		error_on(ret, "there are %d errors while registering devices", ret);
		goto out;
	}

	for( i = devstart ; i < argc ; i++ ){
		char *path;

		if (is_block_device(argv[i]) != 1) {
			error("not a block device: %s", argv[i]);
			ret = 1;
			goto out;
		}
		path = canonicalize_path(argv[i]);
		if (!path) {
			error("could not canonicalize path '%s': %m", argv[i]);
			ret = 1;
			goto out;
		}
		printf("Scanning for Btrfs filesystems in '%s'\n", path);
		if (btrfs_register_one_device(path) != 0) {
			ret = 1;
			free(path);
			goto out;
		}
		free(path);
	}

out:
	return !!ret;
}

static const char * const cmd_device_ready_usage[] = {
	"btrfs device ready <device>",
	"Check device to see if it has all of its devices in cache for mounting",
	NULL
};

static int cmd_device_ready(int argc, char **argv)
{
	struct	btrfs_ioctl_vol_args args;
	int	fd;
	int	ret;
	char	*path;

	clean_args_no_options(argc, argv, cmd_device_ready_usage);

	if (check_argc_exact(argc - optind, 1))
		usage(cmd_device_ready_usage);

	fd = open("/dev/btrfs-control", O_RDWR);
	if (fd < 0) {
		perror("failed to open /dev/btrfs-control");
		return 1;
	}

	path = canonicalize_path(argv[optind]);
	if (!path) {
		error("could not canonicalize pathname '%s': %m",
			argv[optind]);
		ret = 1;
		goto out;
	}

	if (is_block_device(path) != 1) {
		error("not a block device: %s", path);
		ret = 1;
		goto out;
	}

	memset(&args, 0, sizeof(args));
	strncpy_null(args.name, path);
	ret = ioctl(fd, BTRFS_IOC_DEVICES_READY, &args);
	if (ret < 0) {
		error("unable to determine if device '%s' is ready for mount: %m",
			path);
		ret = 1;
	}

out:
	free(path);
	close(fd);
	return ret;
}

static const char * const cmd_device_stats_usage[] = {
	"btrfs device stats [options] <path>|<device>",
	"Show device IO error statistics",
	"Show device IO error statistics for all devices of the given filesystem",
	"identified by PATH or DEVICE. The filesystem must be mounted.",
	"",
	"-c|--check             return non-zero if any stat counter is not zero",
	"-z|--reset             show current stats and reset values to zero",
	NULL
};

static int cmd_device_stats(int argc, char **argv)
{
	char *dev_path;
	struct btrfs_ioctl_fs_info_args fi_args;
	struct btrfs_ioctl_dev_info_args *di_args = NULL;
	int ret;
	int fdmnt;
	int i;
	int err = 0;
	int check = 0;
	__u64 flags = 0;
	DIR *dirstream = NULL;

	optind = 0;
	while (1) {
		int c;
		static const struct option long_options[] = {
			{"check", no_argument, NULL, 'c'},
			{"reset", no_argument, NULL, 'z'},
			{NULL, 0, NULL, 0}
		};

		c = getopt_long(argc, argv, "cz", long_options, NULL);
		if (c < 0)
			break;

		switch (c) {
		case 'c':
			check = 1;
			break;
		case 'z':
			flags = BTRFS_DEV_STATS_RESET;
			break;
		case '?':
		default:
			usage(cmd_device_stats_usage);
		}
	}

	if (check_argc_exact(argc - optind, 1))
		usage(cmd_device_stats_usage);

	dev_path = argv[optind];

	fdmnt = open_path_or_dev_mnt(dev_path, &dirstream, 1);
	if (fdmnt < 0)
		return 1;

	ret = get_fs_info(dev_path, &fi_args, &di_args);
	if (ret) {
		error("getting device info for %s failed: %s", dev_path,
			strerror(-ret));
		err = 1;
		goto out;
	}
	if (!fi_args.num_devices) {
		error("no devices found");
		err = 1;
		goto out;
	}

	for (i = 0; i < fi_args.num_devices; i++) {
		struct btrfs_ioctl_get_dev_stats args = {0};
		char path[BTRFS_DEVICE_PATH_NAME_MAX + 1];

		strncpy(path, (char *)di_args[i].path,
			BTRFS_DEVICE_PATH_NAME_MAX);
		path[BTRFS_DEVICE_PATH_NAME_MAX] = 0;

		args.devid = di_args[i].devid;
		args.nr_items = BTRFS_DEV_STAT_VALUES_MAX;
		args.flags = flags;

		if (ioctl(fdmnt, BTRFS_IOC_GET_DEV_STATS, &args) < 0) {
			error("device stats ioctl failed on %s: %m",
			      path);
			err |= 1;
		} else {
			char *canonical_path;
			int j;
			static const struct {
				const char name[32];
				u64 num;
			} dev_stats[] = {
				{ "write_io_errs", BTRFS_DEV_STAT_WRITE_ERRS },
				{ "read_io_errs", BTRFS_DEV_STAT_READ_ERRS },
				{ "flush_io_errs", BTRFS_DEV_STAT_FLUSH_ERRS },
				{ "corruption_errs",
					BTRFS_DEV_STAT_CORRUPTION_ERRS },
				{ "generation_errs",
					BTRFS_DEV_STAT_GENERATION_ERRS },
			};

			canonical_path = canonicalize_path(path);

			/* No path when device is missing. */
			if (!canonical_path) {
				canonical_path = malloc(32);
				if (!canonical_path) {
					error("not enough memory for path buffer");
					goto out;
				}
				snprintf(canonical_path, 32,
					 "devid:%llu", args.devid);
			}

			for (j = 0; j < ARRAY_SIZE(dev_stats); j++) {
				/* We got fewer items than we know */
				if (args.nr_items < dev_stats[j].num + 1)
					continue;
				printf("[%s].%-16s %llu\n", canonical_path,
					dev_stats[j].name,
					(unsigned long long)
					 args.values[dev_stats[j].num]);
				if ((check == 1)
				    && (args.values[dev_stats[j].num] > 0))
					err |= 64;
			}

			free(canonical_path);
		}
	}

out:
	free(di_args);
	close_file_or_dir(fdmnt, dirstream);

	return err;
}

static const char * const cmd_device_usage_usage[] = {
	"btrfs device usage [options] <path> [<path>..]",
	"Show detailed information about internal allocations in devices.",
	HELPINFO_UNITS_SHORT_LONG,
	NULL
};

static int _cmd_device_usage(int fd, const char *path, unsigned unit_mode)
{
	int i;
	int ret = 0;
	struct chunk_info *chunkinfo = NULL;
	struct device_info *devinfo = NULL;
	int chunkcount = 0;
	int devcount = 0;

	ret = load_chunk_and_device_info(fd, &chunkinfo, &chunkcount, &devinfo,
			&devcount);
	if (ret)
		goto out;

	for (i = 0; i < devcount; i++) {
		printf("%s, ID: %llu\n", devinfo[i].path, devinfo[i].devid);
		print_device_sizes(&devinfo[i], unit_mode);
		print_device_chunks(&devinfo[i], chunkinfo, chunkcount,
				unit_mode);
		printf("\n");
	}

out:
	free(devinfo);
	free(chunkinfo);

	return ret;
}

static int cmd_device_usage(int argc, char **argv)
{
	unsigned unit_mode;
	int ret = 0;
	int i;

	unit_mode = get_unit_mode_from_arg(&argc, argv, 1);

	clean_args_no_options(argc, argv, cmd_device_usage_usage);

	if (check_argc_min(argc - optind, 1))
		usage(cmd_device_usage_usage);

	for (i = optind; i < argc; i++) {
		int fd;
		DIR *dirstream = NULL;

		if (i > 1)
			printf("\n");

		fd = btrfs_open_dir(argv[i], &dirstream, 1);
		if (fd < 0) {
			ret = 1;
			break;
		}

		ret = _cmd_device_usage(fd, argv[i], unit_mode);
		close_file_or_dir(fd, dirstream);

		if (ret)
			break;
	}

	return !!ret;
}

static const char device_cmd_group_info[] =
"manage and query devices in the filesystem";

const struct cmd_group device_cmd_group = {
	device_cmd_group_usage, device_cmd_group_info, {
		{ "add", cmd_device_add, cmd_device_add_usage, NULL, 0 },
		{ "delete", cmd_device_delete, cmd_device_delete_usage, NULL,
			CMD_ALIAS },
		{ "remove", cmd_device_remove, cmd_device_remove_usage, NULL, 0 },
		{ "scan", cmd_device_scan, cmd_device_scan_usage, NULL, 0 },
		{ "ready", cmd_device_ready, cmd_device_ready_usage, NULL, 0 },
		{ "stats", cmd_device_stats, cmd_device_stats_usage, NULL, 0 },
		{ "usage", cmd_device_usage,
			cmd_device_usage_usage, NULL, 0 },
		NULL_CMD_STRUCT
	}
};

int cmd_device(int argc, char **argv)
{
	return handle_command_group(&device_cmd_group, argc, argv);
}
