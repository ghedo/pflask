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

#include <stdbool.h>
#include <unistd.h>

#include <sched.h>

#include <sys/stat.h>
#include <sys/wait.h>

#include "user.h"
#include "sync.h"
#include "printf.h"
#include "util.h"

void setup_console_owner(char *path, struct user *u) {
    int rc;

    pid_t pid;

    int sync[2];

    struct stat sb;

    uid_t hostuid = geteuid();
    gid_t hostgid = getegid();

    uid_t rootuid;
    gid_t rootgid;

    struct user *users = NULL;

    unsigned int tmp;

    if (!user_get_mapped_root(u, 'u', &tmp))
        fail_printf("No mapping for container root user");

    rootuid = (uid_t) tmp;

    if (!user_get_mapped_root(u, 'g', &tmp))
        fail_printf("No mapping for container root group");

    rootgid = (uid_t) tmp;

    if (!geteuid()) {
        rc = chown(path, rootuid, rootgid);
        sys_fail_if(rc < 0, "Error chowning '%s'", path);

        return;
    }

    if (rootuid == geteuid())
        return;

    if (stat(path, &sb) < 0)
        sysf_printf("stat(%s)", path);

    if (sb.st_uid == geteuid()  && chown(path, -1, hostgid) < 0)
        sysf_printf("Error chgrping '%s'", path);

    user_add_map(&users, 'u', 0, rootuid, 1);
    user_add_map(&users, 'u', hostuid, hostuid, 1);
    user_add_map(&users, 'g', 0, rootgid, 1);
    user_add_map(&users, 'g', (gid_t) sb.st_gid,
                 rootgid + (gid_t) sb.st_gid, 1);
    user_add_map(&users, 'g', hostgid, hostgid, 1);

    sync_init(sync);

    pid = fork();
    if (!pid) {
        _free_ char *chown_cmd = NULL;

        unshare(CLONE_NEWNS | CLONE_NEWUSER);

        sync_barrier_parent(sync, SYNC_START);

        sync_close(sync);

        setup_user("root");

        rc = chown(path, 0, sb.st_gid);
        sys_fail_if(rc < 0, "Error chowning '%s'", path);

        exit(0);
    }

    sync_wait_child(sync, SYNC_START);

    setup_user_map(users, pid);

    sync_wake_child(sync, SYNC_DONE);

    sync_close(sync);

    waitpid(pid, &rc, 0);

    return;
}
