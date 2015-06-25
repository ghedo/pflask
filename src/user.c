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

#include "ut/utlist.h"

#include "user.h"
#include "path.h"
#include "printf.h"
#include "util.h"

struct user {
	char type;

	uid_t id;
	uid_t host_id;
	size_t count;

	struct user *next, *prev;
};

void user_add_map(struct user **users, char type, uid_t id, uid_t host_id,
                  size_t count) {
	struct user *usr = malloc(sizeof(struct user));
	if (usr == NULL) fail_printf("OOM");

	usr->type    = type;
	usr->id      = id;
	usr->host_id = host_id;
	usr->count   = count;

	DL_APPEND(*users, usr);
}

void setup_user_map2(struct user *users, char type, pid_t pid) {
	int rc;

	struct user *i;

	_free_ char *map = strdup("");

	_free_ char *have_cmd = on_path("newuidmap", NULL);

	if (!have_cmd && geteuid() != 0)
		fail_printf("Unprivileged containers need the newuidmap/newgidmap executables");

	DL_FOREACH(users, i) {
		char *tmp = NULL;

		if (i->type != type)
			continue;

		rc = asprintf(&tmp, "%s%u %u %lu%c", map,
		              i->id, i->host_id, i->count,
		              have_cmd ? ' ' : '\n');
		if (rc < 0) fail_printf("OOM");
		freep(&map);

		map = tmp;
	}

	if (have_cmd != NULL) {
		_free_ char *cmd = NULL;

		rc = asprintf(&map, "new%cidmap %u %s", type, pid, map);
		if (rc < 0) fail_printf("OOM");

		rc = system(map);
		if (rc != 0) fail_printf("system(idmap): returned %d", rc);
	} else {
		_close_ int map_fd = -1;

		_free_ char *map_file = NULL;

		rc = asprintf(&map_file, "/proc/%d/%cid_map", pid, type);
		if (rc < 0) fail_printf("OOM");

		map_fd = open(map_file, O_RDWR);
		if (map_fd < 0) sysf_printf("open(%s)", map_file);

		rc = write(map_fd, map, strlen(map));
		if (rc < 0) sysf_printf("write(%c %s)", type, map);
	}
}

void setup_user_map(struct user *users, pid_t pid) {
	setup_user_map2(users, 'u', pid);
	setup_user_map2(users, 'g', pid);
}

void setup_user(const char *user) {
	int rc;

	uid_t pw_uid;
	uid_t pw_gid;

	if (!user_get_uid_gid(user, &pw_uid, &pw_gid))
		return;

	rc = setresgid(pw_gid, pw_gid, pw_gid);
	if (rc < 0) sysf_printf("setresgid()");

	rc = setresuid(pw_uid, pw_uid, pw_uid);
	if (rc < 0) sysf_printf("setresuid()");

	rc = setgroups(0, NULL);
	if (rc < 0) sysf_printf("setgroups()");
}

bool user_get_uid_gid(const char *user, uid_t *uid, gid_t *gid) {
	struct passwd *pwd;

	*uid = 0;
	*gid = 0;

	if (strncmp(user, "root", 5)) {
		errno = 0;

		pwd = getpwnam(user);
		if (!pwd && !errno) {
			err_printf("Invalid user '%s'", user);
			return false;
		}

		if (!pwd && errno)
			sysf_printf("getpwnam()");

		*uid = pwd->pw_uid;
		*gid = pwd->pw_gid;
	}

	return true;
}
