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
#include <stdbool.h>
#include <string.h>

#include <sys/mount.h>
#include <sys/stat.h>

#include <linux/version.h>

#include "ut/utlist.h"

#include "mount.h"
#include "path.h"
#include "printf.h"
#include "util.h"

struct mount {
	char *src;
	char *dst;
	char *type;
	unsigned long flags;
	void *data;

	struct mount *next, *prev;
};

static void mount_add_overlay(struct mount **mounts, const char *overlay,
                              const char *dst, const char *work);

void mount_add(struct mount **mounts, const char *src, const char *dst,
                      const char *type, unsigned long f, void *d) {
	struct mount *mnt = malloc(sizeof(struct mount));
	if (mnt == NULL) fail_printf("OOM");

	mnt->src   = src  ? strdup(src)  : NULL;
	mnt->dst   = dst  ? strdup(dst)  : NULL;
	mnt->type  = type ? strdup(type) : NULL;
	mnt->flags = f;
	mnt->data  = d    ? strdup(d)    : NULL;

	DL_APPEND(*mounts, mnt);
}

void mount_add_from_spec(struct mount **mounts, const char *spec) {
	int rc;

	size_t c;

	_free_ char **opts = NULL;

	_free_ char *tmp = strdup(spec);
	if (tmp == NULL) fail_printf("OOM");

	c = split_str(tmp, &opts, ",");
	if (c == 0) fail_printf("Invalid mount spec '%s'", spec);

	if (strncmp(opts[0], "bind", 4) == 0) {
		_free_ char *src = NULL;
		_free_ char *dst = NULL;

		if (c < 3) fail_printf("Invalid mount spec '%s'", spec);

		src = realpath(opts[1], NULL);
		if (src == NULL) sysf_printf("realpath()");

		dst = realpath(opts[2], NULL);
		if (dst == NULL) sysf_printf("realpath()");

		mount_add(mounts, src, dst, "bind", MS_BIND, NULL);

		if (strncmp(opts[0], "bind-ro", 8) == 0)
			mount_add(mounts, src, dst, "bind-ro",
			          MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
	} else if (strncmp(opts[0], "aufs", 5) == 0) {
		_free_ char *dst = NULL;
		_free_ char *overlay = NULL;
		_free_ char *aufs_opts = NULL;

		if (c < 3) fail_printf("Invalid mount spec '%s'", spec);

		overlay = realpath(opts[1], NULL);
		if (overlay == NULL) sysf_printf("realpath()");

		dst = realpath(opts[2], NULL);
		if (dst == NULL) sysf_printf("realpath()");

		rc = asprintf(&aufs_opts, "br:%s=rw:%s=ro", overlay, dst);
		if (rc < 0) fail_printf("OOM");

		mount_add(mounts, NULL, dst, "aufs", 0, aufs_opts);
	} else if (strncmp(opts[0], "overlay", 8) == 0) {
		_free_ char *dst = NULL;
		_free_ char *overlay = NULL;
		_free_ char *workdir = NULL;

		if (c < 4) fail_printf("Invalid mount spec '%s'", spec);

		overlay = realpath(opts[1], NULL);
		if (overlay == NULL) sysf_printf("realpath()");

		dst = realpath(opts[2], NULL);
		if (dst == NULL) sysf_printf("realpath()");

		workdir = realpath(opts[3], NULL);
		if (workdir == NULL) sysf_printf("realpath()");

		mount_add_overlay(mounts, overlay, dst, workdir);
	} else if (strncmp(opts[0], "tmp", 4) == 0) {
		_free_ char *dst = NULL;

		if (c < 2) fail_printf("Invalid mount spec '%s'", spec);

		dst = realpath(opts[1], NULL);
		if (dst == NULL) sysf_printf("realpath()");

		mount_add(mounts, "tmpfs", dst, "tmpfs", 0, NULL);
	} else {
		fail_printf("Invalid mount type '%s'", opts[0]);
	}
}

void setup_mount(struct mount *mounts, const char *dest, bool is_volatile) {
	int rc;

	struct mount *sys_mounts = NULL;
	struct mount *i = NULL;

	_free_ char *mount_spec = NULL;

	_free_ char *root_dir = NULL;
	_free_ char *work_dir = NULL;

	char template[] = "/tmp/pflask-volatile-XXXXXX";

	rc = mount(NULL, "/", NULL, MS_SLAVE | MS_REC, NULL);
	if (rc < 0) sysf_printf("mount(MS_SLAVE)");

	if (dest != NULL) {
		if (is_volatile) {
			if (!mkdtemp(template))
				sysf_printf("mkdtemp()");

			rc = mount("tmpfs", template, "tmpfs", 0, NULL);
			if (rc < 0) sysf_printf("mount(tmpfs)");

			root_dir = prefix_root(template, "root");
			rc = mkdir(root_dir, 0755);
			if (rc < 0)
				sysf_printf("mkdir(%s)", work_dir);

			work_dir = prefix_root(template, "work");
			rc = mkdir(work_dir, 0755);
			if (rc < 0)
				sysf_printf("mkdir(%s)", work_dir);

			mount_add_overlay(&sys_mounts, root_dir, dest, work_dir);
		}

		mount_add(&sys_mounts, "proc", "/proc", "proc",
		          MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL);

		mount_add(&sys_mounts, prefix_root(dest, "/proc/sys"),
		          "/proc/sys", "proc/sys", MS_BIND, NULL);

		mount_add(&sys_mounts, NULL, "/proc/sys", "proc/sys-ro",
		          MS_BIND | MS_RDONLY | MS_REMOUNT, NULL);

		mount_add(&sys_mounts, "sysfs", "/sys", "sysfs",
		          MS_NOSUID | MS_NOEXEC | MS_NODEV | MS_RDONLY, NULL);

		mount_add(&sys_mounts, "tmpfs", "/dev", "tmpfs",
		          MS_NOSUID | MS_STRICTATIME, "mode=755");

		mount_add(&sys_mounts, "devpts", "/dev/pts", "devpts",
		          MS_NOSUID | MS_NOEXEC,
		          "newinstance,ptmxmode=0666,mode=0620,gid=5");

		mount_add(&sys_mounts, "tmpfs", "/dev/shm", "tmpfs",
		          MS_NOSUID | MS_STRICTATIME | MS_NODEV, "mode=1777");

		mount_add(&sys_mounts, "tmpfs", "/run", "tmpfs",
		          MS_NOSUID | MS_NODEV | MS_STRICTATIME, "mode=755");
	}

	DL_CONCAT(sys_mounts, mounts);

	DL_FOREACH(sys_mounts, i) {
		_free_ char *mnt_dest = prefix_root(dest, i->dst);

		rc = mkdir(mnt_dest, 0755);
		if (rc < 0) {
			struct stat sb;

			switch (errno) {
			case EEXIST:
				if (!stat(mnt_dest, &sb) &&
				    !S_ISDIR(sb.st_mode))
					fail_printf("Not a directory");
				break;

			default:
				sysf_printf("mkdir(%s)", mnt_dest);
				break;
			}
		}

		rc = mount(i->src, mnt_dest, i->type, i->flags, i->data);
		if (rc < 0) sysf_printf("mount(%s)", i->type);
	}
}

static void mount_add_overlay(struct mount **mounts, const char *overlay,
                              const char *dst, const char *workdir) {
	int rc;

	_free_ char *overlayfs_opts = NULL;

#ifdef HAVE_AUFS
	rc = asprintf(&overlayfs_opts, "br:%s=rw:%s=ro", overlay, dst);
	if (rc < 0) fail_printf("OOM");

	mount_add(mounts, NULL, dst, "aufs", 0, overlayfs_opts);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
	rc = asprintf(&overlayfs_opts,
		      "upperdir=%s,lowerdir=%s,workdir=%s",
		      overlay, dst, workdir);
	if (rc < 0) fail_printf("OOM");

	mount_add(mounts, NULL, dst, "overlay", 0, overlayfs_opts);
#else
	fail_printf("The 'overlay' mount type is not supported");
#endif
}
