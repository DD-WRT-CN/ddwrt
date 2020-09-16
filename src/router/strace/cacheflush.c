/*
 * Copyright (c) 1999 Andreas Schwab <schwab@issan.cs.uni-dortmund.de>
 * Copyright (c) 2010 Mike Frysinger <vapier@gentoo.org>
 * Copyright (c) 2010 Carmelo Amoroso <carmelo.amoroso@st.com>
 * Copyright (c) 2015 Ezequiel Garcia <ezequiel@vanguardiasur.com.ar>
 * Copyright (c) 2014-2015 Dmitry V. Levin <ldv@altlinux.org>
 * Copyright (c) 2014-2019 The strace developers.
 * All rights reserved.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "defs.h"

#ifdef HAVE_ASM_CACHECTL_H
# include <asm/cachectl.h>
#endif

#ifdef M68K
# include "xlat/cacheflush_scope.h"
# include "xlat/cacheflush_flags.h"

SYS_FUNC(cacheflush)
{
	/* addr */
	printaddr(tcp->u_arg[0]);
	tprints(", ");
	/* scope */
	printxval(cacheflush_scope, tcp->u_arg[1], "FLUSH_SCOPE_???");
	tprints(", ");
	/* flags */
	printflags(cacheflush_flags, tcp->u_arg[2], "FLUSH_CACHE_???");
	/* len */
	tprintf(", %lu", tcp->u_arg[3]);

	return RVAL_DECODED;
}
#endif /* M68K */

#if defined(BFIN) || defined(CSKY)
# include "xlat/cacheflush_flags.h"

SYS_FUNC(cacheflush)
{
	/* start addr */
	printaddr(tcp->u_arg[0]);
	/* length */
	tprintf(", %lu, ", tcp->u_arg[1]);
	/* flags */
	printxval(cacheflush_flags, tcp->u_arg[2], "?CACHE");

	return RVAL_DECODED;
}
#endif /* BFIN || CSKY */

#ifdef SH
# include "xlat/cacheflush_flags.h"

SYS_FUNC(cacheflush)
{
	/* addr */
	printaddr(tcp->u_arg[0]);
	/* len */
	tprintf(", %lu, ", tcp->u_arg[1]);
	/* flags */
	printflags(cacheflush_flags, tcp->u_arg[2], "CACHEFLUSH_???");

	return RVAL_DECODED;
}
#endif /* SH */

#ifdef NIOS2
SYS_FUNC(cacheflush)
{
	/* addr */
	printaddr(tcp->u_arg[0]);
	/* len */
	tprintf(", %lu, ", tcp->u_arg[3]);
	/* scope and flags (cache type) are currently ignored */

	return RVAL_DECODED;
}
#endif /* NIOS2 */
