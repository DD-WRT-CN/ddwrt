/*
 * Copyright (C) 2019 Ernesto A. Fernández <ernesto.mnd.fernandez@gmail.com>
 */

#include <linux/fs.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <apfs/raw.h>
#include "mkapfs.h"
#include "super.h"

int fd;
struct parameters *param;
static char *progname;

/**
 * usage - Print usage information and exit
 */
static void usage(void)
{
	fprintf(stderr,
		"usage: %s [-L label] [-U UUID] [-u UUID] [-sv] "
		"device [blocks]\n",
		progname);
	exit(1);
}

/**
 * version - Print version information and exit
 */
static void version(void)
{
	printf("mkapfs version 0.1\n");
	exit(1);
}

/**
 * system_error - Print a system error message and exit
 */
__attribute__((noreturn)) void system_error(void)
{
	perror(progname);
	exit(1);
}

/**
 * get_device_size - Get the block count of the device or image being checked
 * @blocksize: the filesystem blocksize
 */
static u64 get_device_size(unsigned int blocksize)
{
	struct stat buf;
	u64 size;

	if (fstat(fd, &buf))
		system_error();

	if ((buf.st_mode & S_IFMT) == S_IFREG)
		return buf.st_size / blocksize;

	if (ioctl(fd, BLKGETSIZE64, &size))
		system_error();
	return size / blocksize;
}

/**
 * get_random_uuid - Get a random UUID string in standard format
 *
 * Returns a pointer to the string.
 */
static char *get_random_uuid(void)
{
	char *uuid;
	ssize_t ret;

	/* Length of a null-terminated UUID standard format string */
	uuid = malloc(37);
	if (!uuid)
		system_error();

	/* Linux provides randomly generated UUIDs at /proc */
	do {
		int uuid_fd;

		uuid_fd = open("/proc/sys/kernel/random/uuid", O_RDONLY);
		if (uuid_fd == -1)
			system_error();

		ret = read(uuid_fd, uuid, 36);
		if (ret == -1)
			system_error();

		close(uuid_fd);
	} while (ret != 36);

	/* Put a null-termination, just in case */
	uuid[36] = 0;
	return uuid;
}

/**
 * complete_parameters - Set all uninitialized parameters to their defaults
 *
 * Also runs any needed checks on the parameters provided by the user.
 */
static void complete_parameters(void)
{
	u64 dev_block_count;

	if (!param->blocksize)
		param->blocksize = APFS_NX_DEFAULT_BLOCK_SIZE;

	dev_block_count = get_device_size(param->blocksize);
	if (!param->block_count)
		param->block_count = dev_block_count;
	if (param->block_count > dev_block_count) {
		fprintf(stderr, "%s: device is not big enough\n", progname);
		exit(1);
	}
	if (param->block_count * param->blocksize < 128 * 1024 * 1024) {
		fprintf(stderr, "%s: small containers are not supported\n",
			progname);
		exit(1);
	}

	/* Make sure the volume label fits, along with its null termination */
	if (param->label && strlen(param->label) + 1 > APFS_VOLNAME_LEN)
		fprintf(stderr, "%s: volume label is too long\n", progname);

	if (!param->main_uuid)
		param->main_uuid = get_random_uuid();
	if (!param->vol_uuid)
		param->vol_uuid = get_random_uuid();
}

int main(int argc, char *argv[])
{
	char *filename;

	progname = argv[0];
	param = calloc(1, sizeof(*param));
	if (!param)
		system_error();

	while (1) {
		int opt = getopt(argc, argv, "L:U:u:sv");

		if (opt == -1)
			break;

		switch (opt) {
		case 'L':
			param->label = optarg;
			break;
		case 'U':
			param->main_uuid = optarg;
			break;
		case 'u':
			param->vol_uuid = optarg;
			break;
		case 's':
			param->case_sensitive = true;
			break;
		case 'v':
			version();
		default:
			usage();
		}
	}

	if (optind == argc - 2) {
		filename = argv[optind];
		/* TODO: reject malformed numbers? */
		param->block_count = atoll(argv[optind + 1]);
	} else if (optind == argc - 1) {
		filename = argv[optind];
	} else {
		usage();
	}

	fd = open(filename, O_RDWR);
	if (fd == -1)
		system_error();
	complete_parameters();

	make_container();
	return 0;
}
