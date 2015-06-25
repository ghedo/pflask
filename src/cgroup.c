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

#include "ut/utlist.h"

#include "printf.h"
#include "util.h"

#define CGROUP_BASE "/sys/fs/cgroup"

struct cgroup {
	char *controller;
	char *name;

	struct cgroup *next, *prev;
};

static void create_cgroup(const char *controller, const char *name);
static void attach_cgroup(const char *controller, const char *name, pid_t pid);
static void destroy_cgroup(const char *controller, const char *name);

void cgroup_add(struct cgroup **groups, char *controller) {
	int rc;

	pid_t pid = getpid();

	struct cgroup *cg = malloc(sizeof(struct cgroup));
	if (cg == NULL) fail_printf("OOM");

	cg->controller = strdup(controller);

	rc = asprintf(&cg->name, "pflask.%d", pid);
	if (rc < 0) fail_printf("OOM");

	DL_APPEND(*groups, cg);
}

void setup_cgroup(struct cgroup *groups, pid_t pid) {
	struct cgroup *i = NULL;

	DL_FOREACH(groups, i) {
		create_cgroup(i->controller, i->name);
		attach_cgroup(i->controller, i->name, pid);
	}
}

void clean_cgroup(struct cgroup *groups) {
	struct cgroup *i = NULL;

	DL_FOREACH(groups, i) {
		destroy_cgroup(i->controller, i->name);
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
