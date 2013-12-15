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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include <sys/syscall.h>

#include <sched.h>
#include <linux/sched.h>

#include <signal.h>
#include <getopt.h>

#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "dev.h"
#include "pty.h"
#include "user.h"
#include "mount.h"
#include "netif.h"
#include "printf.h"
#include "util.h"
#include "version.h"

static int clone_flags = SIGCHLD      |
			 CLONE_NEWNS  |
			 CLONE_NEWIPC |
			 CLONE_NEWPID |
			 CLONE_NEWUSER|
			 CLONE_NEWUTS;

static struct option long_opts[] = {
	{ "mount",     required_argument, NULL, 'm' },
	{ "netif",     optional_argument, NULL, 'n' },
	{ "user",      required_argument, NULL, 'u' },
	{ "root",      required_argument, NULL, 'r' },
	{ "chdir",     required_argument, NULL, 'c' },
	{ "detach",    no_argument,       NULL, 'd' },
	{ "attach",    required_argument, NULL, 'a' },
	{ "no-userns", no_argument,       NULL, 'U' },
	{ "no-mountns", no_argument,      NULL, 'M' },
	{ "no-netns",  no_argument,       NULL, 'N' },
	{ "no-ipcns",  no_argument,       NULL, 'I' },
	{ "no-utsns",  no_argument,       NULL, 'H' },
	{ "no-pidns",  no_argument,       NULL, 'P' },
	{ "help",      no_argument,       NULL, 'h' },
	{ 0, 0, 0, 0 }
};

static void do_chroot(char *dest);
static pid_t do_clone(void);

static inline void help(void);
static inline void version(void);

int main(int argc, char *argv[]) {
	int rc, i;

	pid_t pid = -1;
	uid_t uid = -1;
	gid_t gid = -1;

	_free_ char *user   = NULL;
	_free_ char *dest   = NULL;
	_free_ char *change = NULL;

	_close_ int master_fd = -1;
	char *master_name;

	int   detach = 0;

	siginfo_t status;

	const char *short_opts = "+m:n::u:r:c:da:UMNIHPh";

	while ((rc = getopt_long(argc, argv, short_opts, long_opts, &i)) !=-1) {
		switch (rc) {
			case 'm':
				add_mount_from_spec(optarg);
				break;

			case 'n':
				clone_flags |= CLONE_NEWNET;
				add_netif_from_spec(optarg);
				break;

			case 'u':
				user = strdup(optarg);
				break;

			case 'r':
				dest = realpath(optarg, NULL);
				if (dest == NULL) sysf_printf("realpath()");
				break;

			case 'c':
				change = strdup(optarg);
				break;

			case 'd':
				detach = 1;
				break;

			case 'a': {
				char *end;
				pid = strtol(optarg, &end, 10);
				if (*end != '\0')
					fail_printf(
						"Invalid option '%s'",optarg);
				break;
			}

			case 'U':
				clone_flags &= ~(CLONE_NEWUSER);
				break;

			case 'M':
				clone_flags &= ~(CLONE_NEWNS);
				break;

			case 'N':
				clone_flags &= ~(CLONE_NEWNET);
				break;

			case 'I':
				clone_flags &= ~(CLONE_NEWIPC);
				break;

			case 'H':
				clone_flags &= ~(CLONE_NEWUTS);
				break;

			case 'P':
				clone_flags &= ~(CLONE_NEWPID);
				break;

			case '?':
			case 'h':
				help();
				return 0;
		}
	}

	if (pid != -1) {
		master_fd = recv_pty(pid);
		if (master_fd < 0) fail_printf("Invalid PID '%u'", pid);

		pid = -1;
		goto process;
	}

	if (user == NULL) {
		user = strdup("root");
		if (user == NULL) fail_printf("OOM");
	}

	open_master_pty(&master_fd, &master_name);

	uid = getuid();
	gid = getgid();

	if (detach) {
		openlog("pflask", LOG_NDELAY | LOG_PID, LOG_DAEMON);
		use_syslog = 1;

		rc = daemon(0, 0);
		if (rc < 0) sysf_printf("daemon()");
	}

	pid = do_clone();

	if (pid == 0) {
		/* TODO: register with machined */
		/* TODO: cgroup */

		rc = close(master_fd);
		if (rc < 0) sysf_printf("close()");

		open_slave_pty(master_name);

		rc = setsid();
		if (rc < 0) sysf_printf("setsid()");

		rc = prctl(PR_SET_PDEATHSIG, SIGKILL);
		if (rc < 0) sysf_printf("prctl(PR_SET_PDEATHSIG)");

		if (clone_flags & CLONE_NEWUSER)
			map_user_to_user(uid, gid, user);

		do_mount(dest);

		if (dest != NULL) {
			copy_nodes(dest);

			make_ptmx(dest);

			make_symlinks(dest);

			make_console(dest, master_name);

			do_chroot(dest);
		}

		umask(0022);

		/* TODO: drop capabilities */

		do_user(user);

		if (change) {
			rc = chdir(change);
			if (rc < 0) sysf_printf("chdir()");
		}

		if (dest != NULL) {
			char *term = getenv("TERM");

			clearenv();

			setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin", 1);
			setenv("USER", user, 1);
			setenv("LOGNAME", user, 1);
			setenv("TERM", term, 1);
		}

		setenv("container", "pflask", 1);

		if (argc > optind)
			rc = execvpe(argv[optind], argv + optind, environ);
		else
			rc = execle("/bin/bash", "-bash", NULL, environ);

		if (rc < 0) sysf_printf("exec()");
	}

	do_netif(pid);

process:
	if (detach)
		serve_pty(master_fd);
	else
		process_pty(master_fd);

	if (pid == -1)
		return 0;

	rc = waitid(P_PID, pid, &status, WEXITED);
	if (rc < 0) sysf_printf("waitid()");

	switch (status.si_code) {
		case CLD_EXITED:
			if (status.si_status != 0)
				err_printf("Child failed with code '%d'",
					   status.si_status);
			else
				ok_printf("Child exited");
			break;

		case CLD_KILLED:
			err_printf("Child was terminated");
			break;

		default:
			err_printf("Child failed");
			break;
	}

	return status.si_status;
}

static void do_chroot(char *dest) {
	int rc;

	rc = chdir(dest);
	if (rc < 0) sysf_printf("chdir()");

	rc = chroot(".");
	if (rc < 0) sysf_printf("chroot()");

	rc = chdir("/");
	if (rc < 0) sysf_printf("chdir(/)");
}

static pid_t do_clone(void) {
	pid_t pid;

	pid = syscall(__NR_clone, clone_flags, NULL);
	if (pid < 0) {
		if (errno == EINVAL) {
			clone_flags &= ~(CLONE_NEWUSER);
			pid = syscall(__NR_clone, clone_flags, NULL);
		}
	}

	if (pid < 0) sysf_printf("clone()");

	return pid;
}

static inline void version(void) {
	printf("pflask v%s\n", PFLASK_VERSION);
}

static inline void help(void) {
	#define CMD_HELP(CMDL, CMDS, MSG) printf("  %s, %-15s \t%s.\n", COLOR_YELLOW CMDS, CMDL COLOR_OFF, MSG);

	printf(COLOR_RED "Usage: " COLOR_OFF);
	printf(COLOR_GREEN "pflask " COLOR_OFF);
	puts("[OPTIONS] [COMMAND [ARGS...]]\n");

	puts(COLOR_RED " Options:" COLOR_OFF);

	CMD_HELP("--mount", "-m",
		"Create a new mount point inside the container");
	CMD_HELP("--netif", "-n",
		"Create a new network namespace and optionally move a network interface inside it");

	CMD_HELP("--user",  "-u",
		"Run the command as the specified user inside the container");

	CMD_HELP("--root",  "-r",
		"Use the specified directory as root inside the container");
	CMD_HELP("--chdir", "-c",
		"Change to the specified directory inside the container");

	CMD_HELP("--detach", "-d",
		"Detach from terminal");
	CMD_HELP("--attach", "-a",
		"Attach to the specified detached process");

	puts("");

	CMD_HELP("--no-userns",  "-U", "Disable user namespace support");
	CMD_HELP("--no-mountns", "-M", "Disable mount namespace support");
	CMD_HELP("--no-netns",   "-N", "Disable net namespace support");
	CMD_HELP("--no-ipcns",   "-I", "Disable IPC namespace support");
	CMD_HELP("--no-utsns",   "-H", "Disable UTS namespace support");
	CMD_HELP("--no-pidrns",  "-P", "Disable PID namespace support");

	puts("");
}
