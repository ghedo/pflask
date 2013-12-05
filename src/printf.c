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

#include "printf.h"

void ok_printf(const char *fmt, ...) {
	va_list args;

	/* if (cfg.silent || cfg.quiet) */
	/* 	return; */

	va_start(args, fmt);
	fprintf(stderr, "[" COLOR_GREEN "✔" COLOR_OFF "] ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

void debug_printf(const char *fmt, ...) {
	va_list args;

	/* if (!cfg.verbose || cfg.silent || cfg.quiet) */
	/* 	return; */

	va_start(args, fmt);
	fprintf(stderr,  "[" COLOR_YELLOW "¡" COLOR_OFF "] ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

void err_printf(const char *fmt, ...) {
	va_list args;

	/* if (cfg.silent) */
	/* 	return; */

	va_start(args, fmt);
	fprintf(stderr,  "[" COLOR_RED "✘" COLOR_OFF "] ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
}

void fail_printf(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	fprintf(stderr,  "[" COLOR_RED "✘" COLOR_OFF "] ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");

	_exit(-1);
}

void sysf_printf(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	fprintf(stderr,  "[" COLOR_RED "✘" COLOR_OFF "] ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, ": %s\n", strerror(errno));

	exit(-1);
}
