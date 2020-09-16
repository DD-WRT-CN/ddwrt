/*
 * Copyright (c) 2017-2018 Dmitry V. Levin <ldv@altlinux.org>
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef STRACE_RT_SIGFRAME_H
# define STRACE_RT_SIGFRAME_H

# include <signal.h>

typedef struct {
	unsigned int	tramp[9];
	siginfo_t	info;
	ucontext_t	uc;
} struct_rt_sigframe;

#endif /* !STRACE_RT_SIGFRAME_H */
