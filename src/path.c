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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "printf.h"
#include "util.h"

int path_compare(const char *a, const char *b) {
	int d;

	/* A relative path and an abolute path must not compare as equal.
	 * Which one is sorted before the other does not really matter.
	 * Here a relative path is ordered before an absolute path. */
	d = (a[0] == '/') - (b[0] == '/');
	if (d)
		return d;

	for (;;) {
		size_t j, k;

		a += strspn(a, "/");
		b += strspn(b, "/");

		if (*a == 0 && *b == 0)
			return 0;

		/* Order prefixes first: "/foo" before "/foo/bar" */
		if (*a == 0)
			return -1;
		if (*b == 0)
			return 1;

		j = strcspn(a, "/");
		k = strcspn(b, "/");

		/* Alphabetical sort: "/foo/aaa" before "/foo/b" */
		d = memcmp(a, b, MIN(j, k));
		if (d)
			return (d > 0) - (d < 0); /* sign of d */

		/* Sort "/foo/a" before "/foo/aaa" */
		d = (j > k) - (j < k);  /* sign of (j - k) */
		if (d)
			return d;

		a += j;
		b += k;
	}
}

char *path_prefix_root(const char *root, const char *path) {
	char *n, *p;
	size_t l;

	/* If root is passed, prefixes path with it. Otherwise returns
	 * it as is. */

	/* First, drop duplicate prefixing slashes from the path */
	while (path[0] == '/' && path[1] == '/')
		path++;

	if (!root || !root[0] ||
	    (path_compare(root, "/") == 0) ||
	    (path_compare(root, path) == 0))
		return strdup(path);

	l = strlen(root) + 1 + strlen(path) + 1;

	n = malloc(l);
	if (!n)
		return NULL;

	p = stpcpy(n, root);

	while (p > n && p[-1] == '/')
		p--;

	if (path[0] != '/')
		*(p++) = '/';

	strcpy(p, path);
	return n;
}

char *on_path(char *cmd, const char *rootfs) {
	int rc;

	_free_ char *path = NULL;

	char *iter = NULL;
	char *entry = NULL;
	char *saveptr = NULL;
	char *cmd_path = NULL;

	path = getenv("PATH");
	if (!path)
		return NULL;

	path = strdup(path);
	if (!path)
		return NULL;

	iter = path;

	while ((entry = strtok_r(iter, ":", &saveptr))) {
		iter = NULL;

		if (rootfs)
			rc = asprintf(&cmd_path, "%s/%s/%s", rootfs, entry, cmd);
		else
			rc = asprintf(&cmd_path, "%s/%s", entry, cmd);

		if (rc >= 0 && !access(cmd_path, X_OK))
			return cmd_path;
	}

	return NULL;
}

bool path_is_absolute(const char *p) {
	return p[0] == '/';
}
