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
#include <string.h>

#include <sys/stat.h>

#include "printf.h"
#include "util.h"

#define CGROUP_BASE "/sys/fs/cgroup"

static void create_cgroup(const char *controller, const char *name);
static void attach_cgroup(const char *controller, const char *name, pid_t pid);
static void destroy_cgroup(const char *controller, const char *name);

void validate_cgroup_spec(const char *spec) {
	int rc;

	_free_ char *tmp = NULL;
	_free_ char **controllers = NULL;

	if (spec == NULL)
		return;

	tmp = strdup(spec);
	if (tmp == NULL) fail_printf("OOM");

	size_t c = split_str(tmp, &controllers, ",");
	if (c == 0) fail_printf("Invalid cgroup spec '%s'", spec);

	for (size_t i = 0; i < c; i++) {
		struct stat sb;
		_free_ char *path = NULL;

		rc = asprintf(&path, CGROUP_BASE "/%s", controllers[i]);
		if (rc < 0) fail_printf("OOM");

		rc = stat(path, &sb);
		if (rc < 0) goto invalid_controller;

		if (!S_ISDIR(sb.st_mode))
			goto invalid_controller;

		continue;

		invalid_controller:
			fail_printf("Invalid cgroup controller '%s'",
							controllers[i]);
	}
}

void do_cgroup(const char *spec, pid_t pid) {
	int rc;

	_free_ char *tmp = NULL;
	_free_ char **controllers = NULL;

	if (spec == NULL)
		return;

	tmp = strdup(spec);
	if (tmp == NULL) fail_printf("OOM");

	size_t c = split_str(tmp, &controllers, ",");
	if (c == 0) fail_printf("Invalid cgroup spec '%s'", spec);

	for (size_t i = 0; i < c; i++) {
		_free_ char *name = NULL;

		rc = asprintf(&name, "pflask.%d", pid);
		if (rc < 0) fail_printf("OOM");

		create_cgroup(controllers[i], name);
		attach_cgroup(controllers[i], name, 1);
	}
}

void undo_cgroup(const char *spec, pid_t pid) {
	int rc;

	_free_ char *tmp = NULL;
	_free_ char **controllers = NULL;

	if (spec == NULL)
		return;

	tmp = strdup(spec);
	if (tmp == NULL) fail_printf("OOM");

	size_t c = split_str(tmp, &controllers, ",");
	if (c == 0) fail_printf("Invalid cgroup spec '%s'", spec);

	for (size_t i = 0; i < c; i++) {
		_free_ char *name = NULL;

		rc = asprintf(&name, "pflask.%d", pid);
		if (rc < 0) fail_printf("OOM");

		destroy_cgroup(controllers[i], name);
	}
}

static void create_cgroup(const char *controller, const char *name) {
	int rc;

	_free_ char *path = NULL;

	rc = asprintf(&path, CGROUP_BASE "/%s/%s", controller, name);
	if (rc < 0) fail_printf("OOM");

	rc = mkdir(path, 0755);
	if (rc < 0) {
		switch (errno) {
			case EEXIST:
				return;

			default:
				sysf_printf("Error creating cgroup");
		}
	}
}

static void attach_cgroup(const char *controller, const char *name, pid_t pid) {
	int rc;

	FILE *tasks = NULL;
	_free_ char *path = NULL;

	rc = asprintf(&path, CGROUP_BASE "/%s/%s/tasks", controller, name);
	if (rc < 0) fail_printf("OOM");

	tasks = fopen(path, "w");
	if (tasks == NULL) sysf_printf("fopen()");

	fprintf(tasks, "%d\n", pid);

	rc = fclose(tasks);
	if (rc < 0) sysf_printf("fclose()");
}

static void destroy_cgroup(const char *controller, const char *name) {
	int rc;

	_free_ char *path = NULL;

	rc = asprintf(&path, CGROUP_BASE "/%s/%s", controller, name);
	if (rc < 0) fail_printf("OOM");

	rc = rmdir(path);
	if (rc < 0) sysf_printf("Error destroying cgroup");
}
