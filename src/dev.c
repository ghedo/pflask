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
#include <fcntl.h>
#include <unistd.h>

#include <sys/mount.h>
#include <sys/stat.h>

#include "printf.h"
#include "util.h"

void make_ptmx(char *dest) {
	int rc;

	_free_ char *target = NULL;

	rc = asprintf(&target, "%s/dev/ptmx", dest);
	if (rc < 0) fail_printf("OOM");

	rc = symlink("/dev/pts/ptmx", target);
	if (rc < 0) sysf_printf("symlink()");
}

void make_console(char *dest, char *console) {
	int rc;

	struct stat sb;
	_free_ char *target = NULL;

	rc = chmod(console, 0600);
	if (rc < 0) sysf_printf("chmod()");

	rc = chown(console, 0, 0);
	if (rc < 0) sysf_printf("chown()");

	rc = stat(console, &sb);
	if (rc < 0) sysf_printf("stat()");

	rc = asprintf(&target, "%s/dev/console", dest);
	if (rc < 0) fail_printf("OOM");

	rc = mknod(target, sb.st_mode, sb.st_rdev);
	if (rc < 0) sysf_printf("mknod()");

	rc = mount(console, target, NULL, MS_BIND, NULL);
	if (rc < 0) sysf_printf("mount()");
}

void make_symlinks(char *dest) {
	int rc;

	const char *src[] = {
		"/proc/kcore",
		"/proc/self/fd",
		"/proc/self/fd/0",
		"/proc/self/fd/1",
		"/proc/self/fd/2"
	};

	const char *dst[] = {
		"/dev/core",
		"/dev/fd",
		"/dev/stdin",
		"/dev/stdout",
		"/dev/stderr"
	};

	for (size_t i = 0; i <  sizeof(src) / sizeof(*src); i++) {
		_free_ char *link = NULL;

		rc = asprintf(&link, "%s/%s", dest, dst[i]);
		if (rc < 0) fail_printf("OOM");

		rc = symlink(src[i], link);
		if (rc < 0) sysf_printf("symlink()");
	}
}

void copy_nodes(char *dest) {
	int rc;

	mode_t u = umask(0000);

	const char *nodes[] = {
		"/dev/tty",
		"/dev/full",
		"/dev/null",
		"/dev/zero",
		"/dev/random",
		"/dev/urandom"
	};

	for (size_t i = 0; i <  sizeof(nodes) / sizeof(*nodes); i++) {
		struct stat sb;
		_free_ char *target = NULL;

		rc = asprintf(&target, "%s%s", dest, nodes[i]);
		if (rc < 0) fail_printf("OOM");

		rc = stat(nodes[i], &sb);
		if (rc < 0) sysf_printf("stat()");

		rc = mknod(target, sb.st_mode, sb.st_rdev);
		if (rc < 0) sysf_printf("mknod()");
	}

	umask(u);
}
