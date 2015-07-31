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

#include <stdlib.h>
#include <unistd.h>

#define MIN(a, b) ((a) > (b) ? (b) : (a))

#define fail_if_(cond, fmt, ...)				\
	do {							\
		if ((cond))					\
			fail_printf(fmt, __VA_ARGS__);		\
	} while (0)
#define fail_if(...) fail_if_(__VA_ARGS__, "")

#define sys_fail_if_(cond, fmt, ...)				\
	do {							\
		if ((cond))					\
			sysf_printf(fmt, __VA_ARGS__);		\
	} while (0)
#define sys_fail_if(...) sys_fail_if_(__VA_ARGS__, "")

#define _free_ __attribute__((cleanup(freep)))
#define _close_ __attribute__((cleanup(closep)))

static inline void freep(void *p) {
	if (!p) return;

	free(*(void **) p);

	*(void **)p = NULL;
}

static inline void closep(int *p) {
	int rc;

	if (*p == -1)
		return;

	rc = close(*p);
	sys_fail_if(rc < 0, "Error closing fd");

	*p = -1;
}

size_t split_str(char *orig, char ***dest, char *needle);
