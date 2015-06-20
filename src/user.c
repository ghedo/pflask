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
#include <errno.h>
#include <string.h>

#include <pwd.h>
#include <grp.h>

#include "printf.h"
#include "util.h"

void map_user_to_user(uid_t uid, gid_t gid, const char *user) {
	int rc;

	_close_ int setgroups_fd = -1;
	_close_ int uid_map_fd = -1;
	_close_ int gid_map_fd = -1;

	_free_ char *uid_map = NULL;
	_free_ char *gid_map = NULL;

	uid_t pw_uid;
	uid_t pw_gid;

	struct passwd *pwd;

	if (strncmp(user, "root", 5) == 0) {
		pw_uid = 0;
		pw_gid = 0;
	} else {
		errno = 0;
		pwd = getpwnam(user);
		if (pwd == NULL) {
			if (errno) sysf_printf("getpwnam()");
			else       fail_printf("Invalid user '%s'", user);
		}

		pw_uid = pwd->pw_uid;
		pw_gid = pwd->pw_gid;
	}

	setgroups_fd = open("/proc/self/setgroups", O_RDWR);
	if (setgroups_fd >= 0) {
		rc = write(setgroups_fd, "deny", 4);
		if (rc < 0) sysf_printf("write(setgroups)");
	}

	rc = asprintf(&uid_map, "%d %d 1", pw_uid, uid);
	if (rc < 0) fail_printf("OOM");

	uid_map_fd = open("/proc/self/uid_map", O_RDWR);
	if (uid_map_fd < 0) sysf_printf("open(uid_map)");

	rc = write(uid_map_fd, uid_map, strlen(uid_map));
	if (rc < 0) sysf_printf("write(uid_map)");

	rc = asprintf(&gid_map, "%d %d 1", pw_gid, gid);
	if (rc < 0) fail_printf("OOM");

	gid_map_fd = open("/proc/self/gid_map", O_RDWR);
	if (gid_map_fd < 0) sysf_printf("open(gid_map)");

	rc = write(gid_map_fd, gid_map, strlen(gid_map));
	if (rc < 0) sysf_printf("write(gid_map)");
}

void do_user(const char *user) {
	int rc;

	uid_t pw_uid;
	uid_t pw_gid;

	struct passwd *pwd;

	if (strncmp(user, "root", 5) == 0) {
		pw_uid = 0;
		pw_gid = 0;
	} else {
		errno = 0;
		pwd = getpwnam(user);
		if (pwd == NULL) {
			if (errno) sysf_printf("getpwnam()");
			else       fail_printf("Invalid user '%s'", user);
		}

		pw_uid = pwd->pw_uid;
		pw_gid = pwd->pw_gid;
	}

	rc = setresgid(pw_gid, pw_gid, pw_gid);
	if (rc < 0) sysf_printf("setresgid()");

	rc = setresuid(pw_uid, pw_uid, pw_uid);
	if (rc < 0) sysf_printf("setresuid()");
}
