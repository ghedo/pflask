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
#include <string.h>
#include <unistd.h>

#include <sys/mount.h>
#include <sys/stat.h>

#include "printf.h"
#include "util.h"

typedef struct MOUNT_LIST {
	char *src;
	char *dst;
	char *type;
	unsigned long flags;
	void *data;

	struct MOUNT_LIST *next;
} mount_list;

static mount_list *mounts = NULL;

void add_mount(char *src, char *dst, char *type, unsigned long f, void *d) {
	mount_list *mnt = malloc(sizeof(mount_list));
	if (mnt == NULL) fail_printf("OOM");

	mnt -> src   = src  ? strdup(src)  : NULL;
	mnt -> dst   = dst  ? strdup(dst)  : NULL;
	mnt -> type  = type ? strdup(type) : NULL;
	mnt -> flags = f;
	mnt -> data  = d    ? strdup(d)    : NULL;

	mnt -> next  = NULL;

	if (mounts)
		mnt -> next = mounts;

	mounts = mnt;
}

void add_mount_inside(char *base, char *src, char *dst, char *type,
					unsigned long f, void *d) {
	int rc;

	_free_ char *target = NULL;

	rc = asprintf(&target, "%s%s", base, dst);
	if (rc < 0) fail_printf("OOM");

	add_mount(src, target, type, f, d);
}

void add_mount_from_spec(char *spec) {
	int rc;
	_free_ char **opts = NULL;

	_free_ char *tmp = strdup(spec);
	if (tmp == NULL) fail_printf("OOM");

	size_t c = split_str(tmp, &opts, ",");
	if (c == 0) fail_printf("Invalid mount spec '%s'", spec);

	if (strncmp(opts[0], "bind", 5) == 0) {
		char *src, *dst;

		if (c < 3) fail_printf("Invalid mount spec '%s'", spec);

		src = realpath(opts[1], NULL);
		if (src == NULL) sysf_printf("realpath()");

		dst = realpath(opts[2], NULL);
		if (dst == NULL) sysf_printf("realpath()");

		add_mount(src, dst, NULL, MS_BIND, NULL);
	} else if (strncmp(opts[0], "aufs", 4) == 0) {
		char *dst, *overlay;
		_free_ char *aufs_opts = NULL;

		if (c < 3) fail_printf("Invalid mount spec '%s'", spec);

		overlay = realpath(opts[1], NULL);
		if (overlay == NULL) sysf_printf("realpath()");

		dst = realpath(opts[2], NULL);
		if (dst == NULL) sysf_printf("realpath()");

		rc = asprintf(&aufs_opts, "br:%s=rw:%s=ro", overlay, dst);
		if (rc < 0) fail_printf("OOM");

		add_mount(NULL, dst, "aufs", 0, aufs_opts);
	} else if (strncmp(opts[0], "loop", 5) == 0) {
		/* TODO: loop device support */
	} else if (strncmp(opts[0], "tmp", 4) == 0) {
		char *dst;

		if (c < 2) fail_printf("Invalid mount spec '%s'", spec);

		dst = realpath(opts[1], NULL);
		if (dst == NULL) sysf_printf("realpath()");

		add_mount("tmpfs", dst, "tmpfs", 0, NULL);
	} else {
		fail_printf("Invalid mount type '%s'", opts[0]);
	}
}

void do_mount(char *dest) {
	int rc;

	mount_list *i = NULL;

	rc = mount(NULL, "/", NULL, MS_SLAVE | MS_REC, NULL);
	if (rc < 0) sysf_printf("mount(MS_SLAVE)");

	if (dest != NULL) {
		add_mount(dest, dest, NULL, MS_BIND, NULL);

		add_mount_inside(dest, "proc", "/proc", "proc",
			MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL);

		add_mount_inside(dest, "/proc/sys", "/proc/sys", NULL,
			MS_BIND, NULL);

		add_mount_inside(dest, NULL, "/proc/sys", NULL,
			MS_BIND | MS_RDONLY | MS_REMOUNT, NULL);

		add_mount_inside(dest, "sysfs", "/sys", "sysfs",
			MS_NOSUID | MS_NOEXEC | MS_NODEV | MS_RDONLY, NULL);

		add_mount_inside(dest, "tmpfs", "/dev", "tmpfs",
			MS_NOSUID | MS_STRICTATIME, "mode=755");

		add_mount_inside(dest, "devpts", "/dev/pts", "devpts",
			MS_NOSUID | MS_NOEXEC,
			"newinstance,ptmxmode=000,mode=620,gid=5");

		add_mount_inside(dest, "tmpfs", "/dev/shm", "tmpfs",
			MS_NOSUID | MS_STRICTATIME | MS_NODEV, "mode=1777");

		add_mount_inside(dest, "tmpfs", "/run", "tmpfs",
			MS_NOSUID | MS_NODEV | MS_STRICTATIME, "mode=755");

		/* add_mount(dest, "/", NULL, MS_MOVE, NULL); */
	}

	while (mounts) {
		mount_list *next = mounts -> next;
		mounts -> next = i;
		i = mounts;
		mounts = next;
	}

	while (i != NULL) {
		struct stat sb;

		if (stat(i -> dst, &sb) < 0) {
			rc = mkdir(i -> dst, 0755);
			if (rc < 0) sysf_printf("mkdir(%s)", i -> dst);
		}

		rc = mount(i->src, i -> dst, i -> type, i -> flags, i -> data);
		if (rc < 0) sysf_printf("mount(%s)", i -> type);

		i = i -> next;
	}
}
