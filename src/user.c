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
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <pwd.h>
#include <grp.h>

#include "path.h"
#include "printf.h"
#include "util.h"

void enable_setgroups(bool enable, pid_t pid) {
	int rc;

	_close_ int setgroups_fd = -1;

	_free_ char *setgroups_file = NULL;

	rc = asprintf(&setgroups_file, "/proc/%d/setgroups", pid);
	if (rc < 0) fail_printf("OOM");

	setgroups_fd = open(setgroups_file, O_RDWR);
	if (setgroups_fd >= 0) {
		char *cmd = (enable) ? "allow" : "deny";
		rc = write(setgroups_fd, cmd, strlen(cmd));
		if (rc < 0) sysf_printf("write(setgroups)");
	}
}

void map_users(char type, uid_t id, uid_t host_id, size_t count, pid_t pid) {
	int rc;

	_free_ char *map = NULL;
	_free_ char *cmd = on_path("newuidmap", NULL);

	if (cmd != NULL) {
		rc = asprintf(&map, "new%cidmap %u %u %u %lu",
		              type, pid, id, host_id, count);
		if (rc < 0) fail_printf("OOM");

		rc = system(map);
		if (rc != 0) fail_printf("system(map): returned %d", rc);
	} else {
		_free_ char *map_file = NULL;

		_close_ int map_fd = -1;

		err_printf("newuidmap not found, falling back to direct /proc access");

		rc = asprintf(&map, "%u %u %lu", id, host_id, count);
		if (rc < 0) fail_printf("OOM");

		rc = asprintf(&map_file, "/proc/%d/%cid_map", pid, type);
		if (rc < 0) fail_printf("OOM");

		map_fd = open(map_file, O_RDWR);
		if (map_fd < 0) sysf_printf("open(%s)", map_file);

		rc = write(map_fd, map, strlen(map));
		if (rc < 0) sysf_printf("write()");
	}
}

void get_uid_gid(const char *user, uid_t *uid, gid_t *gid) {
	struct passwd *pwd;

	*uid = 0;
	*gid = 0;

	if (strncmp(user, "root", 5)) {
		errno = 0;

		pwd = getpwnam(user);
		if (pwd == NULL) {
			if (errno) sysf_printf("getpwnam()");
			else       fail_printf("Invalid user '%s'", user);
		}

		*uid = pwd->pw_uid;
		*gid = pwd->pw_gid;
	}
}

void map_user_to_user(uid_t uid, gid_t gid, const char *user, pid_t pid) {
	uid_t pw_uid;
	uid_t pw_gid;

	get_uid_gid(user, &pw_uid, &pw_gid);

	enable_setgroups(false, pid);

	map_users('u', pw_uid, uid, 1, pid);
	map_users('g', pw_gid, gid, 1, pid);
}

void setup_user(const char *user) {
	int rc;

	uid_t pw_uid;
	uid_t pw_gid;

	get_uid_gid(user, &pw_uid, &pw_gid);

	rc = setresgid(pw_gid, pw_gid, pw_gid);
	if (rc < 0) sysf_printf("setresgid()");

	rc = setresuid(pw_uid, pw_uid, pw_uid);
	if (rc < 0) sysf_printf("setresuid()");
}
