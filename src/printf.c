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

#include "printf.h"
#include "util.h"

int use_syslog = 0;

void ok_printf(const char *fmt, ...) {
	int rc;
	va_list args;

	_free_ char *prefix = NULL;
	_free_ char *format = NULL;

	if (use_syslog == 0)
		prefix = strdup("[" COLOR_GREEN "✔" COLOR_OFF "]");

	rc = asprintf(&format, "%s%s\n", prefix ? prefix : "", fmt);
	if (rc < 0) fail_printf("OOM");

	va_start(args, fmt);
	if (use_syslog == 1)
		vsyslog(LOG_INFO, format, args);
	else
		vfprintf(stderr, format, args);
	va_end(args);
}

void debug_printf(const char *fmt, ...) {
	int rc;
	va_list args;

	_free_ char *prefix = NULL;
	_free_ char *format = NULL;

	if (use_syslog == 0)
		prefix = strdup("[" COLOR_YELLOW "¡" COLOR_OFF "] ");

	rc = asprintf(&format, "%s%s\n", prefix ? prefix : "", fmt);
	if (rc < 0) fail_printf("OOM");

	va_start(args, fmt);
	if (use_syslog == 1)
		vsyslog(LOG_DEBUG, format, args);
	else
		vfprintf(stderr, format, args);
	va_end(args);
}

void err_printf(const char *fmt, ...) {
	int rc;
	va_list args;

	_free_ char *prefix = NULL;
	_free_ char *format = NULL;

	if (use_syslog == 0)
		prefix = strdup("[" COLOR_RED "✘" COLOR_OFF "] ");

	rc = asprintf(&format, "%s%s\n", prefix ? prefix : "", fmt);
	if (rc < 0) fail_printf("OOM");

	va_start(args, fmt);
	if (use_syslog == 1)
		vsyslog(LOG_ERR, format, args);
	else
		vfprintf(stderr, format, args);
	va_end(args);
}

void fail_printf(const char *fmt, ...) {
	int rc;
	va_list args;

	_free_ char *prefix = NULL;
	_free_ char *format = NULL;

	if (use_syslog == 0)
		prefix = strdup("[" COLOR_RED "✘" COLOR_OFF "] ");

	rc = asprintf(&format, "%s%s\n", prefix ? prefix : "", fmt);
	if (rc < 0) fail_printf("OOM");

	va_start(args, fmt);
	if (use_syslog == 1)
		vsyslog(LOG_CRIT, format, args);
	else
		vfprintf(stderr, format, args);
	va_end(args);

	_exit(-1);
}

void sysf_printf(const char *fmt, ...) {
	int rc;
	va_list args;

	_free_ char *prefix = NULL;
	_free_ char *format = NULL;

	if (use_syslog == 0)
		prefix = strdup("[" COLOR_RED "✘" COLOR_OFF "] ");

	rc = asprintf(&format, "%s%s: %s\n", prefix ? prefix : "", fmt,
							strerror(errno));
	if (rc < 0) fail_printf("OOM");

	va_start(args, fmt);
	if (use_syslog == 1)
		vsyslog(LOG_CRIT, format, args);
	else
		vfprintf(stderr, format, args);
	va_end(args);

	_exit(-1);
}
