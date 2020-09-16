/*
 * Copyright (c) 2004-2006 Maxim Sobolev <sobomax@FreeBSD.org>
 * Copyright (c) 2006-2007 Sippy Software, Inc., http://www.sippysoft.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _RTPP_LOG_STAND_H_
#define _RTPP_LOG_STAND_H_

#include <syslog.h>
#include <stdarg.h>

struct rtpp_log_inst;
struct rtpp_cfg_stable;

#define	rtpp_log_t	struct rtpp_log_inst *

#if 0
#include "rtpp_defines.h"
#endif

#define	RTPP_LOG_DBUG	LOG_DEBUG
#define	RTPP_LOG_INFO	LOG_INFO
#define	RTPP_LOG_WARN	LOG_WARNING
#define	RTPP_LOG_ERR	LOG_ERR
#define	RTPP_LOG_CRIT	LOG_CRIT

#define	rtpp_log_open(cf, app, call_id, flag) _rtpp_log_open(cf, app, call_id);
#define	rtpp_log_write(level, handle, format, args...)			\
	_rtpp_log_write(handle, level, __FUNCTION__, format, ## args)
#define	rtpp_log_ewrite(level, handle, format, args...)			\
	_rtpp_log_ewrite(handle, level, __FUNCTION__, format, ## args)
#define	rtpp_log_close(handle) _rtpp_log_close(handle)

void _rtpp_log_write(struct rtpp_log_inst *, int, const char *, const char *, ...);
void _rtpp_log_ewrite(struct rtpp_log_inst *, int, const char *, const char *, ...);
struct rtpp_log_inst *_rtpp_log_open(struct rtpp_cfg_stable *, const char *, const char *);
void _rtpp_log_close(struct rtpp_log_inst *);
int rtpp_log_str2lvl(const char *);
int rtpp_log_str2fac(const char *);
void rtpp_log_setlevel(struct rtpp_log_inst *, int level);

#endif
