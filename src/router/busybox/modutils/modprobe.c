/* vi: set sw=4 ts=4: */
/*
 * Modprobe written from scratch for BusyBox
 *
 * Copyright (c) 2002 by Robert Griebl, griebl@gmx.de
 * Copyright (c) 2003 by Andrew Dennison, andrew.dennison@motec.com.au
 * Copyright (c) 2005 by Jim Bauer, jfbauer@nfr.com
 *
 * Portions Copyright (c) 2005 by Yann E. MORIN, yann.morin.1998@anciens.enib.fr
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
*/

//applet:IF_MODPROBE(APPLET(modprobe, BB_DIR_SBIN, BB_SUID_DROP))
//usage:#define modprobe_trivial_usage
//usage:	"[-qfwrsv] MODULE [SYMBOL=VALUE]..."
//usage:#define modprobe_full_usage "\n\n"
//usage:       "	-r	Remove MODULE (stacks) or do autoclean"
//usage:     "\n	-q	Quiet"
//usage:     "\n	-v	Verbose"
//usage:     "\n	-f	Force"
//usage:     "\n	-w	Wait for unload"
//usage:     "\n	-s	Report via syslog instead of stderr"

#include "libbb.h"
#include "common_bufsiz.h"
#include <sys/utsname.h>
#include <fnmatch.h>

#define ENABLE_FEATURE_MODPROBE_FANCY_ALIAS 0
#define ENABLE_FEATURE_MODPROBE_MULTIPLE_OPTIONS 0
#define line_buffer bb_common_bufsiz1

struct mod_opt_t {	/* one-way list of options to pass to a module */
	char *  m_opt_val;
	struct mod_opt_t * m_next;
};

struct dep_t {	/* one-way list of dependency rules */
	/* a dependency rule */
	char *  m_name;                     /* the module name*/
	char *  m_path;                     /* the module file path */
	struct mod_opt_t *  m_options;	    /* the module options */

	unsigned int m_isalias      :1;     /* the module is an alias */
	unsigned int m_isblacklisted:1;     /* the module is blacklisted */
	unsigned int m_reserved     :14;    /* stuffin' */

	unsigned int m_depcnt       :16;    /* the number of dependable module(s) */
	char ** m_deparr;                   /* the list of dependable module(s) */

	struct dep_t * m_next;              /* the next dependency rule */
};

struct mod_list_t {	/* two-way list of modules to process */
	/* a module description */
	const char * m_name;
	char * m_path;
	struct mod_opt_t * m_options;

	struct mod_list_t * m_prev;
	struct mod_list_t * m_next;
};

struct include_conf_t {
	struct dep_t *first;
	struct dep_t *current;
};

static struct dep_t *depend;

#define MAIN_OPT_STR "acdklnqrst:vVC:"
#define INSERT_ALL     1        /* a */
#define DUMP_CONF_EXIT 2        /* c */
#define D_OPT_IGNORED  4        /* d */
#define AUTOCLEAN_FLG  8        /* k */
#define LIST_ALL       16       /* l */
#define SHOW_ONLY      32       /* n */
#define QUIET          64       /* q */
#define REMOVE_OPT     128      /* r */
#define DO_SYSLOG      256      /* s */
#define RESTRICT_DIR   512      /* t */
#define VERBOSE        1024     /* v */
#define VERSION_ONLY   2048     /* V */
#define CONFIG_FILE    4096     /* C */

#define autoclean       (option_mask32 & AUTOCLEAN_FLG)
#define show_only       (option_mask32 & SHOW_ONLY)
#define quiet           (option_mask32 & QUIET)
#define remove_opt      (option_mask32 & REMOVE_OPT)
#define do_syslog       (option_mask32 & DO_SYSLOG)
#define verbose         (option_mask32 & VERBOSE)

static int parse_tag_value(char *buffer, char **ptag, char **pvalue)
{
	char *tag, *value;

	buffer = skip_whitespace(buffer);
	tag = value = buffer;
	while (!isspace(*value)) {
		if (!*value)
			return 0;
		value++;
	}
	*value++ = '\0';
	value = skip_whitespace(value);
	if (!*value)
		return 0;

	*ptag = tag;
	*pvalue = value;

	return 1;
}

/*
 * This function appends an option to a list
 */
static struct mod_opt_t *append_option(struct mod_opt_t *opt_list, char *opt)
{
	struct mod_opt_t *ol = opt_list;

	if (ol) {
		while (ol->m_next) {
			ol = ol->m_next;
		}
		ol->m_next = xzalloc(sizeof(struct mod_opt_t));
		ol = ol->m_next;
	} else {
		ol = opt_list = xzalloc(sizeof(struct mod_opt_t));
	}

	ol->m_opt_val = xstrdup(opt);
	/*ol->m_next = NULL; - done by xzalloc*/

	return opt_list;
}

#if ENABLE_FEATURE_MODPROBE_MULTIPLE_OPTIONS
/* static char* parse_command_string(char* src, char **dst);
 *   src: pointer to string containing argument
 *   dst: pointer to where to store the parsed argument
 *   return value: the pointer to the first char after the parsed argument,
 *                 NULL if there was no argument parsed (only trailing spaces).
 *   Note that memory is allocated with xstrdup when a new argument was
 *   parsed. Don't forget to free it!
 */
#define ARG_EMPTY      0x00
#define ARG_IN_DQUOTES 0x01
#define ARG_IN_SQUOTES 0x02
static char *parse_command_string(char *src, char **dst)
{
	int opt_status = ARG_EMPTY;
	char* tmp_str;

	/* Dumb you, I have nothing to do... */
	if (src == NULL) return src;

	/* Skip leading spaces */
	while (*src == ' ') {
		src++;
	}
	/* Is the end of string reached? */
	if (*src == '\0') {
		return NULL;
	}
	/* Reached the start of an argument
	 * By the way, we duplicate a little too much
	 * here but what is too much is freed later. */
	*dst = tmp_str = xstrdup(src);
	/* Get to the end of that argument */
	while (*tmp_str != '\0'
	 && (*tmp_str != ' ' || (opt_status & (ARG_IN_DQUOTES | ARG_IN_SQUOTES)))
	) {
		switch (*tmp_str) {
		case '\'':
			if (opt_status & ARG_IN_DQUOTES) {
				/* Already in double quotes, keep current char as is */
			} else {
				/* shift left 1 char, until end of string: get rid of the opening/closing quotes */
				memmove(tmp_str, tmp_str + 1, strlen(tmp_str));
				/* mark me: we enter or leave single quotes */
				opt_status ^= ARG_IN_SQUOTES;
				/* Back one char, as we need to re-scan the new char there. */
				tmp_str--;
			}
			break;
		case '"':
			if (opt_status & ARG_IN_SQUOTES) {
				/* Already in single quotes, keep current char as is */
			} else {
				/* shift left 1 char, until end of string: get rid of the opening/closing quotes */
				memmove(tmp_str, tmp_str + 1, strlen(tmp_str));
				/* mark me: we enter or leave double quotes */
				opt_status ^= ARG_IN_DQUOTES;
				/* Back one char, as we need to re-scan the new char there. */
				tmp_str--;
			}
			break;
		case '\\':
			if (opt_status & ARG_IN_SQUOTES) {
				/* Between single quotes: keep as is. */
			} else {
				switch (*(tmp_str+1)) {
				case 'a':
				case 'b':
				case 't':
				case 'n':
				case 'v':
				case 'f':
				case 'r':
				case '0':
					/* We escaped a special character. For now, keep
					 * both the back-slash and the following char. */
					tmp_str++;
					src++;
					break;
				default:
					/* We escaped a space or a single or double quote,
					 * or a back-slash, or a non-escapable char. Remove
					 * the '\' and keep the new current char as is. */
					memmove(tmp_str, tmp_str + 1, strlen(tmp_str));
					break;
				}
			}
			break;
		/* Any other char that is special shall appear here.
		 * Example: $ starts a variable
		case '$':
			do_variable_expansion();
			break;
		 * */
		default:
			/* any other char is kept as is. */
			break;
		}
		tmp_str++; /* Go to next char */
		src++; /* Go to next char to find the end of the argument. */
	}
	/* End of string, but still no ending quote */
	if (opt_status & (ARG_IN_DQUOTES | ARG_IN_SQUOTES)) {
		bb_error_msg_and_die("unterminated (single or double) quote in options list: %s", src);
	}
	*tmp_str++ = '\0';
	*dst = xrealloc(*dst, (tmp_str - *dst));
	return src;
}
#else
#define parse_command_string(src, dst)	(0)
#endif /* ENABLE_FEATURE_MODPROBE_MULTIPLE_OPTIONS */

static int is_conf_command(char *buffer, const char *command)
{
	int len = strlen(command);
	return ((strstr(buffer, command) == buffer) &&
			isspace(buffer[len]));
}

/*
 * This function reads aliases and default module options from a configuration file
 * (/etc/modprobe.conf syntax). It supports includes (only files, no directories).
 */

static int FAST_FUNC include_conf_file_act(const char *filename,
					   struct stat *statbuf UNUSED_PARAM,
					   void *userdata,
					   int depth UNUSED_PARAM);

static int FAST_FUNC include_conf_dir_act(const char *filename UNUSED_PARAM,
					  struct stat *statbuf UNUSED_PARAM,
					  void *userdata UNUSED_PARAM,
					  int depth)
{
	if (depth > 1)
		return SKIP;

	return TRUE;
}

static int include_conf_recursive(struct include_conf_t *conf, const char *filename, int flags)
{
	return recursive_action(filename, ACTION_RECURSE | flags,
				include_conf_file_act,
				include_conf_dir_act,
				conf, 1);
}

static int FAST_FUNC include_conf_file_act(const char *filename,
					   struct stat *statbuf UNUSED_PARAM,
					   void *userdata,
					   int depth UNUSED_PARAM)
{
	struct include_conf_t *conf = (struct include_conf_t *) userdata;
	struct dep_t **first = &conf->first;
	struct dep_t **current = &conf->current;
	int continuation_line = 0;
	FILE *f;

	if (bb_basename(filename)[0] == '.')
		return TRUE;

	f = fopen_for_read(filename);
	if (f == NULL)
		return FALSE;

	// alias parsing is not 100% correct (no correct handling of continuation lines within an alias)!

	while (fgets(line_buffer, COMMON_BUFSIZE, f)) {
		int l;

		*strchrnul(line_buffer, '#') = '\0';

		l = strlen(line_buffer);

		while (l && isspace(line_buffer[l-1])) {
			line_buffer[l-1] = '\0';
			l--;
		}

		if (l == 0) {
			continuation_line = 0;
			continue;
		}

		if (continuation_line)
			continue;

		if (is_conf_command(line_buffer, "alias")) {
			char *alias, *mod;

			if (parse_tag_value(line_buffer + 6, &alias, &mod)) {
				/* handle alias as a module dependent on the aliased module */
				if (!*current) {
					(*first) = (*current) = xzalloc(sizeof(struct dep_t));
				} else {
					(*current)->m_next = xzalloc(sizeof(struct dep_t));
					(*current) = (*current)->m_next;
				}
				(*current)->m_name = xstrdup(alias);
				(*current)->m_isalias = 1;

				if ((strcmp(mod, "off") == 0) || (strcmp(mod, "null") == 0)) {
					/*(*current)->m_depcnt = 0; - done by xzalloc */
					/*(*current)->m_deparr = 0;*/
				} else {
					(*current)->m_depcnt = 1;
					(*current)->m_deparr = xmalloc(sizeof(char *));
					(*current)->m_deparr[0] = xstrdup(mod);
				}
				/*(*current)->m_next = NULL; - done by xzalloc */
			}
		} else if (is_conf_command(line_buffer, "options")) {
			char *mod, *opt;

			/* split the line in the module/alias name, and options */
			if (parse_tag_value(line_buffer + 8, &mod, &opt)) {
				struct dep_t *dt;

				/* find the corresponding module */
				for (dt = *first; dt; dt = dt->m_next) {
					if (strcmp(dt->m_name, mod) == 0)
						break;
				}
				if (dt) {
					if (ENABLE_FEATURE_MODPROBE_MULTIPLE_OPTIONS) {
						char* new_opt = NULL;
						while ((opt = parse_command_string(opt, &new_opt))) {
							dt->m_options = append_option(dt->m_options, new_opt);
						}
					} else {
						dt->m_options = append_option(dt->m_options, opt);
					}
				}
			}
		} else if (is_conf_command(line_buffer, "include")) {
			char *includefile;

			includefile = skip_whitespace(line_buffer + 8);
			include_conf_recursive(conf, includefile, 0);
		} else if (ENABLE_FEATURE_MODPROBE_BLACKLIST &&
				(is_conf_command(line_buffer, "blacklist"))) {
			char *mod;
			struct dep_t *dt;

			mod = skip_whitespace(line_buffer + 10);
			for (dt = *first; dt; dt = dt->m_next) {
				if (strcmp(dt->m_name, mod) == 0)
					break;
			}
			if (dt)
				dt->m_isblacklisted = 1;
		}
	} /* while (fgets(...)) */

	fclose(f);
	return TRUE;
}

static int include_conf_file(struct include_conf_t *conf,
			     const char *filename)
{
	return include_conf_file_act(filename, NULL, conf, 0);
}

static int include_conf_file2(struct include_conf_t *conf,
			      const char *filename, const char *oldname)
{
	if (include_conf_file(conf, filename) == TRUE)
		return TRUE;
	return include_conf_file(conf, oldname);
}

/*
 * This function builds a list of dependency rules from /lib/modules/`uname -r`/modules.dep.
 * It then fills every modules and aliases with their default options, found by parsing
 * modprobe.conf (or modules.conf, or conf.modules).
 */
static struct dep_t *build_dep(void)
{
	FILE *f;
	struct utsname un;
	struct include_conf_t conf = { NULL, NULL };
	char *filename;
	int continuation_line = 0;
	int k_version;

	uname(&un); /* never fails */

	k_version = 0;
	if (un.release[0] == '2') {
		k_version = un.release[2] - '0';
	}
	if (k_version==0)
	    k_version = 6;
	filename = xasprintf(CONFIG_DEFAULT_MODULES_DIR"/%s/"CONFIG_DEFAULT_DEPMOD_FILE, un.release);
	f = fopen_for_read(filename);
	if (ENABLE_FEATURE_CLEAN_UP)
		free(filename);
	if (f == NULL) {
		/* Ok, that didn't work.  Fall back to looking in /lib/modules */
		f = fopen_for_read(CONFIG_DEFAULT_MODULES_DIR"/"CONFIG_DEFAULT_DEPMOD_FILE);
		if (f == NULL) {
			bb_simple_error_msg_and_die("cannot parse "CONFIG_DEFAULT_DEPMOD_FILE);
		}
	}

	while (fgets(line_buffer, COMMON_BUFSIZE, f)) {
		int l = strlen(line_buffer);
		char *p = NULL;

		while (l > 0 && isspace(line_buffer[l-1])) {
			line_buffer[l-1] = '\0';
			l--;
		}

		if (l == 0) {
			continuation_line = 0;
			continue;
		}

		/* Is this a new module dep description? */
		if (!continuation_line) {
			/* find the dep beginning */
			char *col = strchr(line_buffer, ':');
			char *dot = col;

			if (col) {
				/* This line is a dep description */
				const char *mods;
				char *modpath;
				char *mod;

				/* Find the beginning of the module file name */
				*col = '\0';
				mods = bb_basename(line_buffer);

				/* find the path of the module */
				modpath = strchr(line_buffer, '/'); /* ... and this is the path */
				if (!modpath)
					modpath = line_buffer; /* module with no path */
				/* find the end of the module name in the file name */
				if (ENABLE_FEATURE_2_6_MODULES &&
				    (k_version > 4) && (col[-3] == '.') &&
				    (col[-2] == 'k') && (col[-1] == 'o'))
					dot = col - 3;
				else if ((col[-2] == '.') && (col[-1] == 'o'))
					dot = col - 2;

				mod = xstrndup(mods, dot - mods);

				/* enqueue new module */
				if (!conf.current) {
					conf.first = conf.current = xzalloc(sizeof(struct dep_t));
				} else {
					conf.current->m_next = xzalloc(sizeof(struct dep_t));
					conf.current = conf.current->m_next;
				}
				conf.current->m_name = mod;
				conf.current->m_path = xstrdup(modpath);
				/*current->m_options = NULL; - xzalloc did it*/
				/*current->m_isalias = 0;*/
				/*current->m_depcnt = 0;*/
				/*current->m_deparr = 0;*/
				/*current->m_next = 0;*/

				p = col + 1;
			} else
				/* this line is not a dep description */
				p = NULL;
		} else
			/* It's a dep description continuation */
			p = line_buffer;

		/* p points to the first dependable module; if NULL, no dependable module */
		if (p && (p = skip_whitespace(p))[0] != '\0') {
			char *end = &line_buffer[l-1];
			const char *deps;
			char *dep;
			char *next;
			int ext = 0;

			while (isblank(*end) || (*end == '\\'))
				end--;

			do {
				/* search the end of the dependency */
				next = strchr(p, ' ');
				if (next) {
					*next = '\0';
					next--;
				} else
					next = end;

				/* find the beginning of the module file name */
				deps = bb_basename(p);
				if (deps == p)
					deps = skip_whitespace(deps);

				/* find the end of the module name in the file name */
				if (ENABLE_FEATURE_2_6_MODULES
				 && (k_version > 4) && (next[-2] == '.')
				 && (next[-1] == 'k') && (next[0] == 'o'))
					ext = 3;
				else if ((next[-1] == '.') && (next[0] == 'o'))
					ext = 2;

				/* Cope with blank lines */
				if ((next - deps - ext + 1) <= 0)
					continue;
				dep = xstrndup(deps, next - deps - ext + 1);

				/* Add the new dependable module name */
				conf.current->m_deparr = xrealloc_vector(conf.current->m_deparr, 2, conf.current->m_depcnt);
				conf.current->m_deparr[conf.current->m_depcnt++] = dep;

				p = next + 2;
			} while (next < end);
		}

		/* is there other dependable module(s) ? */
		continuation_line = (line_buffer[l-1] == '\\');
	} /* while (fgets(...)) */
	fclose(f);

	/*
	 * First parse system-specific options and aliases
	 * as they take precedence over the kernel ones.
	 * >=2.6: we only care about modprobe.conf
	 * <=2.4: we care about modules.conf and conf.modules
	 */
	{
		int r = FALSE;

		if (ENABLE_FEATURE_2_6_MODULES) {
			if (include_conf_file(&conf, "/etc/modprobe.conf"))
				r = TRUE;
			if (include_conf_recursive(&conf, "/etc/modprobe.d", ACTION_QUIET))
				r = TRUE;
		}
		if (ENABLE_FEATURE_2_4_MODULES && !r)
			include_conf_file2(&conf,
					   "/etc/modules.conf",
					   "/etc/conf.modules");
	}

	/* Only 2.6 has a modules.alias file */
	if (ENABLE_FEATURE_2_6_MODULES) {
		/* Parse kernel-declared module aliases */
		filename = xasprintf(CONFIG_DEFAULT_MODULES_DIR"/%s/modules.alias", un.release);
		include_conf_file2(&conf,
				   filename,
				   CONFIG_DEFAULT_MODULES_DIR"/modules.alias");
		if (ENABLE_FEATURE_CLEAN_UP)
			free(filename);

		/* Parse kernel-declared symbol aliases */
		filename = xasprintf(CONFIG_DEFAULT_MODULES_DIR"/%s/modules.symbols", un.release);
		include_conf_file2(&conf,
				   filename,
				   CONFIG_DEFAULT_MODULES_DIR"/modules.symbols");
		if (ENABLE_FEATURE_CLEAN_UP)
			free(filename);
	}

	return conf.first;
}

/* return 1 = loaded, 0 = not loaded, -1 = can't tell */
static int already_loaded(const char *name)
{
	FILE *f;
	int ret;

	f = fopen_for_read("/proc/modules");
	if (f == NULL)
		return -1;

	ret = 0;
	while (fgets(line_buffer, COMMON_BUFSIZE, f)) {
		char *p = line_buffer;
		const char *n = name;

		while (1) {
			char cn = *n;
			char cp = *p;
			if (cp == ' ' || cp == '\0') {
				if (cn == '\0') {
					ret = 1; /* match! */
					goto done;
				}
				break; /* no match on this line, take next one */
			}
			if (cn == '-') cn = '_';
			if (cp == '-') cp = '_';
			if (cp != cn)
				break; /* no match on this line, take next one */
			n++;
			p++;
		}
	}
 done:
	fclose(f);
	return ret;
}

static int mod_process(const struct mod_list_t *list, int do_insert)
{
	int rc = 0;
	char **argv = NULL;
	struct mod_opt_t *opts;
	int argc_malloc; /* never used when CONFIG_FEATURE_CLEAN_UP not defined */
	int argc;

	while (list) {
		argc = 0;
		if (ENABLE_FEATURE_CLEAN_UP)
			argc_malloc = 0;
		/* If CONFIG_FEATURE_CLEAN_UP is not defined, then we leak memory
		 * each time we allocate memory for argv.
		 * But it is (quite) small amounts of memory that leak each
		 * time a module is loaded,  and it is reclaimed when modprobe
		 * exits anyway (even when standalone shell? Yes --vda).
		 * This could become a problem when loading a module with LOTS of
		 * dependencies, with LOTS of options for each dependencies, with
		 * very little memory on the target... But in that case, the module
		 * would not load because there is no more memory, so there's no
		 * problem. */
		/* enough for minimal insmod (5 args + NULL) or rmmod (3 args + NULL) */
		argv = xmalloc(6 * sizeof(char*));
		if (do_insert) {
			if (already_loaded(list->m_name) != 1) {
				argv[argc++] = (char*)"insmod";
				if (ENABLE_FEATURE_2_4_MODULES) {
					if (do_syslog)
						argv[argc++] = (char*)"-s";
					if (autoclean)
						argv[argc++] = (char*)"-k";
					if (quiet)
						argv[argc++] = (char*)"-q";
					else if (verbose) /* verbose and quiet are mutually exclusive */
						argv[argc++] = (char*)"-v";
				}
				argv[argc++] = list->m_path;
				if (ENABLE_FEATURE_CLEAN_UP)
					argc_malloc = argc;
				opts = list->m_options;
				while (opts) {
					/* Add one more option */
					argc++;
					argv = xrealloc(argv, (argc + 1) * sizeof(char*));
					argv[argc-1] = opts->m_opt_val;
					opts = opts->m_next;
				}
			}
		} else {
			/* modutils uses short name for removal */
			if (already_loaded(list->m_name) != 0) {
				argv[argc++] = (char*)"rmmod";
				if (do_syslog)
					argv[argc++] = (char*)"-s";
				argv[argc++] = (char*)list->m_name;
				if (ENABLE_FEATURE_CLEAN_UP)
					argc_malloc = argc;
			}
		}
		argv[argc] = NULL;

		if (argc) {
			if (verbose) {
				printf("%s module %s\n", do_insert?"Loading":"Unloading", list->m_name);
			}
			if (!show_only) {
				int rc2 = wait4pid(spawn(argv));

				if (do_insert) {
					rc = rc2; /* only last module matters */
				} else if (!rc2) {
					rc = 0; /* success if remove any mod */
				}
			}
			if (ENABLE_FEATURE_CLEAN_UP) {
				/* the last value in the array has index == argc, but
				 * it is the terminating NULL, so we must not free it. */
				while (argc_malloc < argc) {
					free(argv[argc_malloc++]);
				}
			}
		}
		if (ENABLE_FEATURE_CLEAN_UP) {
			free(argv);
			argv = NULL;
		}
		list = do_insert ? list->m_prev : list->m_next;
	}
	return (show_only) ? 0 : rc;
}

/*
 * Check the matching between a pattern and a module name.
 * We need this as *_* is equivalent to *-*, even in pattern matching.
 */
static int check_pattern(const char* pat_src, const char* mod_src)
{
	int ret;

	if (ENABLE_FEATURE_MODPROBE_FANCY_ALIAS) {
		char* pat;
		char* mod;
		char* p;

		pat = xstrdup(pat_src);
		mod = xstrdup(mod_src);

		for (p = pat; (p = strchr(p, '-')); *p++ = '_');
		for (p = mod; (p = strchr(p, '-')); *p++ = '_');

		ret = fnmatch(pat, mod, 0);

		if (ENABLE_FEATURE_CLEAN_UP) {
			free(pat);
			free(mod);
		}

		return ret;
	}
	return fnmatch(pat_src, mod_src, 0);
}

/*
 * Builds the dependency list (aka stack) of a module.
 * head: the highest module in the stack (last to insmod, first to rmmod)
 * tail: the lowest module in the stack (first to insmod, last to rmmod)
 */
static void check_dep(char *mod, struct mod_list_t **head, struct mod_list_t **tail)
{
	struct mod_list_t *find;
	struct dep_t *dt;
	struct mod_opt_t *opt = NULL;
	char *path = NULL;

	/* Search for the given module name amongst all dependency rules.
	 * The module name in a dependency rule can be a shell pattern,
	 * so try to match the given module name against such a pattern.
	 * Of course if the name in the dependency rule is a plain string,
	 * then we consider it a pattern, and matching will still work. */
	for (dt = depend; dt; dt = dt->m_next) {
		if (check_pattern(dt->m_name, mod) == 0) {
			break;
		}
	}

	if (!dt) {
		bb_error_msg("module %s not found", mod);
		return;
	}

	// resolve alias names
	while (dt->m_isalias) {
		if (dt->m_depcnt == 1) {
			struct dep_t *adt;

			for (adt = depend; adt; adt = adt->m_next) {
				if (check_pattern(adt->m_name, dt->m_deparr[0]) == 0 &&
						!(ENABLE_FEATURE_MODPROBE_BLACKLIST &&
							adt->m_isblacklisted))
					break;
			}
			if (adt) {
				/* This is the module we are aliased to */
				struct mod_opt_t *opts = dt->m_options;
				/* Option of the alias are appended to the options of the module */
				while (opts) {
					adt->m_options = append_option(adt->m_options, opts->m_opt_val);
					opts = opts->m_next;
				}
				dt = adt;
			} else {
				bb_error_msg("module %s not found", mod);
				return;
			}
		} else {
			bb_error_msg("bad alias %s", dt->m_name);
			return;
		}
	}

	mod = dt->m_name;
	path = dt->m_path;
	opt = dt->m_options;

	// search for duplicates
	for (find = *head; find; find = find->m_next) {
		if (strcmp(mod, find->m_name) == 0) {
			// found -> dequeue it

			if (find->m_prev)
				find->m_prev->m_next = find->m_next;
			else
				*head = find->m_next;

			if (find->m_next)
				find->m_next->m_prev = find->m_prev;
			else
				*tail = find->m_prev;

			break; // there can be only one duplicate
		}
	}

	if (!find) { // did not find a duplicate
		find = xzalloc(sizeof(struct mod_list_t));
		find->m_name = mod;
		find->m_path = path;
		find->m_options = opt;
	}

	// enqueue at tail
	if (*tail)
		(*tail)->m_next = find;
	find->m_prev = *tail;
	find->m_next = NULL; /* possibly NOT done by xzalloc! */

	if (!*head)
		*head = find;
	*tail = find;

	if (dt) {
		int i;

		/* Add all dependable module for that new module */
		for (i = 0; i < dt->m_depcnt; i++)
			check_dep(dt->m_deparr[i], head, tail);
	}
}

static int mod_insert(char **argv)
{
	struct mod_list_t *tail = NULL;
	struct mod_list_t *head = NULL;
	char *modname = *argv++;
	int rc;

	// get dep list for module mod
	check_dep(modname, &head, &tail);

	rc = 1;
	if (head && tail) {
		while (*argv)
			head->m_options = append_option(head->m_options, *argv++);

		// process tail ---> head
		rc = mod_process(tail, 1);
		if (rc) {
			/*
			 * In case of using udev, multiple instances of modprobe can be
			 * spawned to load the same module (think of two same usb devices,
			 * for example; or cold-plugging at boot time). Thus we shouldn't
			 * fail if the module was loaded, and not by us.
			 */
			if (already_loaded(modname))
				rc = 0;
		}
	}
	return rc;
}

static int mod_remove(char *modname)
{
	static const struct mod_list_t rm_a_dummy = { "-a", NULL, NULL, NULL, NULL };

	int rc;
	struct mod_list_t *head = NULL;
	struct mod_list_t *tail = NULL;

	if (modname)
		check_dep(modname, &head, &tail);
	else  // autoclean
		head = tail = (struct mod_list_t*) &rm_a_dummy;

	rc = 1;
	if (head && tail)
		rc = mod_process(head, 0);  // process head ---> tail
	return rc;
}

int modprobe_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int modprobe_main(int argc UNUSED_PARAM, char **argv)
{
	int rc = EXIT_SUCCESS;
	unsigned opt;
	char *unused;

	opt = getopt32(argv, "^" MAIN_OPT_STR "\0" "q-v:v-q");

	argv += optind;


	if (opt & (DUMP_CONF_EXIT | LIST_ALL))
		return EXIT_SUCCESS;
	if (opt & (RESTRICT_DIR | CONFIG_FILE))
		bb_simple_error_msg_and_die("-t and -C not supported");

	depend = build_dep();

	if (!depend)
		bb_simple_error_msg_and_die("cannot parse "CONFIG_DEFAULT_DEPMOD_FILE);

	if (remove_opt) {
		do {
			/* (*argv) can be NULL here */
			if (mod_remove(*argv)) {
				bb_perror_msg("failed to %s module %s", "remove",
						*argv);
				rc = EXIT_FAILURE;
			}
		} while (*argv && *++argv);
	} else {
		if (!*argv)
			bb_simple_error_msg_and_die("no module or pattern provided");

		if (mod_insert(argv))
			bb_perror_msg_and_die("failed to %s module %s", "load", *argv);
	}

	/* Here would be a good place to free up memory allocated during the dependencies build. */

	return rc;
}
