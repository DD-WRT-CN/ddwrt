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

#ifndef __BTRFS_COMMANDS_H__
#define __BTRFS_COMMANDS_H__

enum {
	CMD_HIDDEN = (1 << 0),	/* should not be in help listings */
	CMD_ALIAS = (1 << 1),	/* alias of next command in cmd_group */
};

struct cmd_struct {
	const char *token;
	int (*fn)(int, char **);

	/*
	 * Usage strings
	 *
	 * A NULL-terminated array of the following format:
	 *
	 *   usagestr[0] - one-line synopsis (required)
	 *   usagestr[1] - one-line short description (required)
	 *   usagestr[2..m] - a long (possibly multi-line) description
	 *                    (optional)
	 *   usagestr[m + 1] - an empty line separator (required if at least one
	 *                     option string is given, not needed otherwise)
	 *   usagestr[m + 2..n] - option strings, one option per line
	 *                        (optional)
	 *   usagestr[n + 1] - NULL terminator
	 *
	 * Options (if present) should always (even if there is no long
	 * description) be prepended with an empty line.  Supplied strings are
	 * indented but otherwise printed as-is, no automatic wrapping is done.
	 *
	 * Grep for cmd_*_usage[] for examples.
	 */
	const char * const *usagestr;

	/* should be NULL if token is not a subgroup */
	const struct cmd_group *next;

	/* CMD_* flags above */
	int flags;
};

#define NULL_CMD_STRUCT {NULL, NULL, NULL, NULL, 0}

struct cmd_group {
	const char * const *usagestr;
	const char *infostr;

	const struct cmd_struct commands[];
};

int handle_command_group(const struct cmd_group *grp, int argc,
			 char **argv);

extern const char * const generic_cmd_help_usage[];

extern const struct cmd_group subvolume_cmd_group;
extern const struct cmd_group filesystem_cmd_group;
extern const struct cmd_group balance_cmd_group;
extern const struct cmd_group device_cmd_group;
extern const struct cmd_group scrub_cmd_group;
extern const struct cmd_group inspect_cmd_group;
extern const struct cmd_group property_cmd_group;
extern const struct cmd_group quota_cmd_group;
extern const struct cmd_group qgroup_cmd_group;
extern const struct cmd_group replace_cmd_group;
extern const struct cmd_group rescue_cmd_group;

extern const char * const cmd_send_usage[];
extern const char * const cmd_receive_usage[];
extern const char * const cmd_check_usage[];
extern const char * const cmd_chunk_recover_usage[];
extern const char * const cmd_super_recover_usage[];
extern const char * const cmd_restore_usage[];
extern const char * const cmd_rescue_usage[];
extern const char * const cmd_inspect_dump_super_usage[];
extern const char * const cmd_inspect_dump_tree_usage[];
extern const char * const cmd_inspect_tree_stats_usage[];
extern const char * const cmd_filesystem_du_usage[];
extern const char * const cmd_filesystem_usage_usage[];

int cmd_subvolume(int argc, char **argv);
int cmd_filesystem(int argc, char **argv);
int cmd_filesystem_du(int argc, char **argv);
int cmd_filesystem_usage(int argc, char **argv);
int cmd_balance(int argc, char **argv);
int cmd_device(int argc, char **argv);
int cmd_scrub(int argc, char **argv);
int cmd_check(int argc, char **argv);
int cmd_chunk_recover(int argc, char **argv);
int cmd_super_recover(int argc, char **argv);
int cmd_inspect(int argc, char **argv);
int cmd_inspect_dump_super(int argc, char **argv);
int cmd_inspect_dump_tree(int argc, char **argv);
int cmd_inspect_tree_stats(int argc, char **argv);
int cmd_property(int argc, char **argv);
int cmd_send(int argc, char **argv);
int cmd_receive(int argc, char **argv);
int cmd_quota(int argc, char **argv);
int cmd_qgroup(int argc, char **argv);
int cmd_replace(int argc, char **argv);
int cmd_restore(int argc, char **argv);
int cmd_select_super(int argc, char **argv);
int cmd_dump_super(int argc, char **argv);
int cmd_debug_tree(int argc, char **argv);
int cmd_rescue(int argc, char **argv);

#endif
