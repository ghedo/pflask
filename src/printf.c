/*
 * The process in the flask.
 *
 * Copyright (c) 2013, Alessandro Ghedini
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <limits.h>

#include "printf.h"
#include "util.h"

int use_syslog = 0;

static void do_log(const char *prefix, const char *fmt, va_list args);

void ok_printf(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	do_log("[" COLOR_GREEN "✔" COLOR_OFF "] ", fmt, args);
	va_end(args);
}

void debug_printf(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	do_log("[" COLOR_YELLOW "¡" COLOR_OFF "] ", fmt, args);
	va_end(args);
}

void err_printf(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	do_log("[" COLOR_RED "✘" COLOR_OFF "] ", fmt, args);
	va_end(args);
}

void fail_printf(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	do_log("[" COLOR_RED "✘" COLOR_OFF "] ", fmt, args);
	va_end(args);

	_exit(-1);
}

void sysf_printf(const char *fmt, ...) {
	int rc;
	va_list args;

	_free_ char *format = NULL;

	rc = asprintf(&format, "%s: %s", fmt, strerror(errno));
	if (rc < 0) fail_printf("OOM");

	va_start(args, fmt);
	do_log("[" COLOR_RED "✘" COLOR_OFF "] ", format, args);
	va_end(args);

	_exit(-1);
}

static void do_log(const char *pre, const char *fmt, va_list args) {
	int rc;
	static char format[LINE_MAX];

	rc = snprintf(format, LINE_MAX, "%s%s\n", use_syslog ? "" : pre, fmt);
	if (rc < 0) fail_printf("EIO");

	if (use_syslog == 1)
		vsyslog(LOG_CRIT, format, args);
	else
		vfprintf(stderr, format, args);
}
