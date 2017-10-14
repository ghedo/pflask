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
#include <fcntl.h>

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

struct overlay {
    char *overlay;
    char *workdir;
    char type;
};

static void make_bind_dest(struct mount *m, const char *dest);
static void make_overlay_opts(struct mount *m, const char *dest);
static void mount_add_overlay(struct mount **mounts, const char *overlay,
                              const char *dst, const char *work);

void mount_add(struct mount **mounts, const char *src, const char *dst,
                      const char *type, unsigned long f, void *d) {
    struct mount *mnt = malloc(sizeof(struct mount));
    fail_if(!mnt, "OOM");

    mnt->src   = src  ? strdup(src)  : NULL;
    mnt->dst   = dst  ? strdup(dst)  : NULL;
    mnt->type  = type ? strdup(type) : NULL;
    mnt->flags = f;

    if (type && !strcmp(type, "overlay"))
        mnt->data = d;
    else
        mnt->data = d ? strdup(d) : NULL;

    DL_APPEND(*mounts, mnt);
}

void mount_add_from_spec(struct mount **mounts, const char *spec) {
    size_t c;

    _free_ char **opts = NULL;

    _free_ char *tmp = strdup(spec);
    fail_if(!tmp, "OOM");

    c = split_str(tmp, &opts, ":");
    fail_if(!c, "Invalid mount spec '%s': not enough args",spec);

    if (!strncmp(opts[0], "bind", 4)) {
        fail_if(c < 3, "Invalid mount spec '%s': not enough args",spec);

        if (!path_is_absolute(opts[1]))
            fail_printf("Invalid mount spec '%s': path not absolute", spec);

        if (!path_is_absolute(opts[2]))
            fail_printf("Invalid mount spec '%s': path not absolute", spec);

        mount_add(mounts, opts[1], opts[2], "bind", MS_BIND, NULL);

        if (!strncmp(opts[0], "bind-ro", 8))
            mount_add(mounts, opts[1], opts[2], "bind-ro",
                      MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
    } else if (!strncmp(opts[0], "overlay", 8)) {
        fail_if(c < 4, "Invalid mount spec '%s': not enough args",spec);

        if (!path_is_absolute(opts[1]))
            fail_printf("Invalid mount spec '%s': path not absolute", spec);

        if (!path_is_absolute(opts[2]))
            fail_printf("Invalid mount spec '%s': path not absolute", spec);

        if (!path_is_absolute(opts[3]))
            fail_printf("Invalid mount spec '%s': path not absolute", spec);

        mount_add_overlay(mounts, opts[1], opts[2], opts[3]);
    } else if (!strncmp(opts[0], "tmp", 4)) {
        fail_if(c < 2, "Invalid mount spec '%s': not enough args",spec);

        if (!path_is_absolute(opts[1]))
            fail_printf("Invalid mount spec '%s': path not absolute", spec);

        mount_add(mounts, "tmpfs", opts[1], "tmpfs", 0, NULL);
    } else {
        fail_printf("Invalid mount type '%s'", opts[0]);
    }
}

void setup_mount(struct mount *mounts, const char *dest, const char *ephemeral_dir) {
    int rc;

    struct mount *sys_mounts = NULL;
    struct mount *i = NULL;

    _free_ char *mount_spec = NULL;

    _free_ char *root_dir = NULL;
    _free_ char *work_dir = NULL;

    _free_ char *procsys_dir = NULL;

    rc = mount(NULL, "/", NULL, MS_SLAVE | MS_REC, NULL);
    sys_fail_if(rc < 0, "Error mounting slave /");

    if (dest != NULL) {
        if (ephemeral_dir != NULL) {
            rc = mount("tmpfs", ephemeral_dir, "tmpfs", 0, NULL);
            sys_fail_if(rc < 0, "Error mounting tmpfs");

            root_dir = path_prefix_root(ephemeral_dir, "root");

            rc = mkdir(root_dir, 0755);
            sys_fail_if(rc < 0, "Error creating directory '%s'",
                                root_dir);

            work_dir = path_prefix_root(ephemeral_dir, "work");

            rc = mkdir(work_dir, 0755);
            sys_fail_if(rc < 0, "Error creating directory '%s'",
                                work_dir);

            mount_add_overlay(&sys_mounts, root_dir, "/", work_dir);
        }

        mount_add(&sys_mounts, "proc", "/proc", "proc",
                  MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL);

        procsys_dir = path_prefix_root(dest, "/proc/sys");
        mount_add(&sys_mounts, procsys_dir, "/proc/sys", "proc/sys",
                  MS_BIND, NULL);

        mount_add(&sys_mounts, NULL, "/proc/sys", "proc/sys-ro",
                  MS_BIND | MS_RDONLY | MS_REMOUNT, NULL);

        mount_add(&sys_mounts, "/sys", "/sys", "bind",
                  MS_BIND | MS_REC | MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);

        mount_add(&sys_mounts, "tmpfs", "/dev", "tmpfs",
                  MS_NOSUID | MS_STRICTATIME, "mode=755");

        mount_add(&sys_mounts, "devpts", "/dev/pts", "devpts",
                  MS_NOSUID | MS_NOEXEC,
                  "newinstance,ptmxmode=0666,mode=0620,gid=5");

        mount_add(&sys_mounts, "tmpfs", "/dev/shm", "tmpfs",
                  MS_NOSUID | MS_STRICTATIME | MS_NODEV, "mode=1777");

        mount_add(&sys_mounts, "tmpfs", "/run", "tmpfs",
                  MS_NOSUID | MS_NODEV | MS_STRICTATIME, "mode=755");

        mount_add(&sys_mounts, "cgroup", "/sys/fs/cgroup", "cgroup2",
                  MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL);
    }

    DL_CONCAT(sys_mounts, mounts);

    DL_FOREACH(sys_mounts, i) {
        _free_ char *mnt_dest = path_prefix_root(dest, i->dst);

        if (!strcmp(i->type, "overlay"))
            make_overlay_opts(i, mnt_dest);

        if (!strcmp(i->type, "bind") || !strcmp(i->type, "bind-ro")) {
            make_bind_dest(i, mnt_dest);
        } else {
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
        }

        rc = mount(i->src, mnt_dest, i->type, i->flags, i->data);
        sys_fail_if(rc < 0, "Error mounting '%s'", i->type);
    }
}

static void make_bind_dest(struct mount *m, const char *dest) {
    int rc;

    struct stat src_sb, dst_sb;

    rc = stat(m->src, &src_sb);
    sys_fail_if(rc < 0, "stat(%s)", m->src);

    if (stat(dest, &dst_sb) >= 0) {
        if (S_ISDIR(src_sb.st_mode) && !S_ISDIR(dst_sb.st_mode))
            fail_printf("Could not bind mount dir %s on file %s",
                        m->src, dest);

        if (!S_ISDIR(src_sb.st_mode) && S_ISDIR(dst_sb.st_mode))
            fail_printf("Could not bind mount file %s on dir %s",
                        m->src, dest);
    } else if (errno == ENOENT) {
        if (!S_ISDIR(src_sb.st_mode)) {
            _close_ int fd = -1;

            fd = open(dest,
                      O_WRONLY | O_CREAT | O_CLOEXEC | O_NOCTTY,
                      0644);

            rc = fd;
        } else {
            rc = mkdir(dest, 0755);
        }

        sys_fail_if(rc < 0, "Could not create mount dest %s", dest);
    } else {
        sysf_printf("Error opening '%s'", dest);
    }
}

static void make_overlay_opts(struct mount *m, const char *dest) {
    int rc;

    _free_ struct overlay *ovl = m->data;

    char *overlay = ovl->overlay;
    char *workdir = ovl->workdir;

    char *overlayfs_opts = NULL;

    if (ovl->type == 'a') {
        rc = asprintf(&overlayfs_opts, "br:%s=rw:%s=ro", overlay, dest);
        fail_if(rc < 0, "OOM");
    } else if (ovl->type == 'o') {
        rc = asprintf(&overlayfs_opts,
                      "upperdir=%s,lowerdir=%s,workdir=%s",
                      overlay, dest, workdir);
        fail_if(rc < 0, "OOM");
    }

    m->data = overlayfs_opts;
}

static void mount_add_overlay(struct mount **mounts, const char *overlay,
                              const char *dst, const char *workdir) {
    struct overlay *ovl = malloc(sizeof(struct overlay));

    ovl->overlay = strdup(overlay);
    ovl->workdir = strdup(workdir);

#ifdef HAVE_AUFS
    ovl->type = 'a';
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0)
    ovl->type = 'o';
#else
    fail_printf("The 'overlay' mount type is not supported");
#endif

    mount_add(mounts, NULL, dst, "overlay", 0, ovl);
}
