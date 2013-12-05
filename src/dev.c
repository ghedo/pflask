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

	rc = stat(console, &sb);
	if (rc < 0) sysf_printf("stat()");

	rc = asprintf(&target, "%s/dev/console", dest);
	if (rc < 0) fail_printf("OOM");

	rc = mknod(target, (sb.st_mode & ~07777) | 0600, sb.st_rdev);
	if (rc < 0) sysf_printf("mknod()");

	rc = mount(console, target, NULL, MS_BIND, NULL);
	if (rc < 0) sysf_printf("mount()");
}

void copy_nodes(char *dest) {
	int i;
	int rc;

	const char *nodes[] = {
		"/dev/tty",
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
