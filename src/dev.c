#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mount.h>
#include <sys/stat.h>

#include "printf.h"
#include "util.h"

void make_ptmx(char *dest) {
	int rc;

	_free_ char *ptmx_dst = NULL;

	rc = asprintf(&ptmx_dst, "%s/dev/ptmx", dest);
	if (rc < 0) fail_printf("OOM");

	rc = symlink("/dev/pts/ptmx", ptmx_dst);
	if (rc < 0) sysf_printf("symlink()");
}

void make_console(char *dest, char *console) {
	int rc;
	struct stat sb;
	_free_ char *target = NULL;

	rc = chmod(console, 0600);
	if (rc < 0) sysf_printf("chmod()");

	rc = chown(console, 0, 0);
	if (rc < 0) sysf_printf("chown()");

	rc = stat(console, &sb);
	if (rc < 0) sysf_printf("stat()");

	rc = asprintf(&target, "%s/dev/console", dest);
	if (rc < 0) fail_printf("OOM");

	rc = mknod(target, sb.st_mode, sb.st_rdev);
	if (rc < 0) sysf_printf("mknod()");

	rc = mount(console, target, NULL, MS_BIND, NULL);
	if (rc < 0) sysf_printf("mount()");
}

void make_symlinks(char *dest) {
	int i, rc;

	const char *src[] = {
		"/proc/kcore",
		"/proc/self/fd",
		"/proc/self/fd/0",
		"/proc/self/fd/1",
		"/proc/self/fd/2"
	};

	const char *dst[] = {
		"/dev/core",
		"/dev/fd",
		"/dev/stdin",
		"/dev/stdout",
		"/dev/stderr"
	};

	for (i = 0; i <  sizeof(src) / sizeof(*src); i++) {
		_free_ char *link = NULL;

		rc = asprintf(&link, "%s/%s", dest, dst[i]);
		if (rc < 0) fail_printf("OOM");

		rc = symlink(src[i], link);
		if (rc < 0) sysf_printf("symlink()");
	}
}

void copy_nodes(char *dest) {
	int i;
	int rc;

	const char *nodes[] = {
		"/dev/tty",
		"/dev/full",
		"/dev/null",
		"/dev/zero",
		"/dev/random",
		"/dev/urandom"
	};

	for (i = 0; i <  sizeof(nodes) / sizeof(*nodes); i++) {
		struct stat sb;
		_free_ char *target = NULL;

		rc = asprintf(&target, "%s%s", dest, nodes[i]);
		if (rc < 0) fail_printf("OOM");

		rc = stat(nodes[i], &sb);
		if (rc < 0) sysf_printf("stat()");

		rc = mknod(target, sb.st_mode, sb.st_rdev);
		if (rc < 0) sysf_printf("mknod()");
	}
}
