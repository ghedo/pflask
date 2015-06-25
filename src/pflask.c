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
#include <syslog.h>

#include <sys/syscall.h>

#include <linux/sched.h>

#include <getopt.h>

#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "dev.h"
#include "pty.h"
#include "user.h"
#include "machine.h"
#include "mount.h"
#include "cgroup.h"
#include "netif.h"
#include "sync.h"
#include "printf.h"
#include "util.h"

static const char *short_opts = "+m:n::u:e:r:wc:g:da:s:kt:UMNIHPh?";

static struct option long_opts[] = {
	{ "mount",     required_argument, NULL, 'm' },
	{ "netif",     optional_argument, NULL, 'n' },
	{ "user",      required_argument, NULL, 'u' },
	{ "map-users", required_argument, NULL, 'e' },
	{ "chroot",    required_argument, NULL, 'r' },
	{ "volatile",  no_argument,       NULL, 'w' },
	{ "chdir",     required_argument, NULL, 'c' },
	{ "cgroup",    required_argument, NULL, 'g' },
	{ "detach",    no_argument,       NULL, 'd' },
	{ "attach",    required_argument, NULL, 'a' },
	{ "setenv",    required_argument, NULL, 's' },
	{ "keepenv",   no_argument,       NULL, 'k' },
	{ "hostname",  required_argument, NULL, 't' },
	{ "no-userns", no_argument,       NULL, 'U' },
	{ "no-mountns", no_argument,      NULL, 'M' },
	{ "no-netns",  no_argument,       NULL, 'N' },
	{ "no-ipcns",  no_argument,       NULL, 'I' },
	{ "no-utsns",  no_argument,       NULL, 'H' },
	{ "no-pidns",  no_argument,       NULL, 'P' },
	{ "help",      no_argument,       NULL, 'h' },
	{ 0, 0, 0, 0 }
};

static size_t validate_optlist(const char *name, const char *opts);

static void do_daemonize(void);
static void do_chroot(const char *dest);
static pid_t do_clone(int *flags);

static inline void help(void);

int main(int argc, char *argv[]) {
	int rc, i;

	int sync[2];

	pid_t pid  = -1;

	uid_t uid = -1;

	uid_t cont_id = -1;
	uid_t host_id = -1;
	size_t id_count = 0;

	_free_ char *user   = strdup("root");
	_free_ char *map    = NULL;
	_free_ char *dest   = NULL;
	_free_ char *change = NULL;
	_free_ char *env    = NULL;
	_free_ char *cgroup = NULL;
	_free_ char *hname  = NULL;

	struct mount *mounts = NULL;
	struct netif *netifs = NULL;
	struct cgroup *cgroups = NULL;

	char *master;
	_close_ int master_fd = -1;

	bool detach  = false;
	bool keepenv = false;
	bool is_volatile = false;

	siginfo_t status;

	int clone_flags = CLONE_NEWNS  |
                          CLONE_NEWIPC |
                          CLONE_NEWPID |
                          CLONE_NEWUTS;

	while ((rc = getopt_long(argc, argv, short_opts, long_opts, &i)) !=-1) {
		switch (rc) {
		case 'm':
			validate_optlist("--mount", optarg);

			mount_add_from_spec(&mounts, optarg);
			break;

		case 'n':
			clone_flags |= CLONE_NEWNET;

			if (optarg != NULL) {
				validate_optlist("--netif", optarg);

				netif_add_from_spec(&netifs, optarg);
			}
			break;

		case 'u':
			clone_flags |= CLONE_NEWUSER;

			freep(&user);

			user = strdup(optarg);
			break;

		case 'e': {
			char *start = optarg, *end = NULL;

			validate_optlist("--map-users", optarg);

			clone_flags |= CLONE_NEWUSER;

			cont_id = strtoul(start, &end, 10);
			if (*end != ':')
				fail_printf("a Invalid value '%s' for --map-users", optarg);

			start = end + 1;

			host_id = strtoul(start, &end, 10);
			if (*end != ':')
				fail_printf("b Invalid value '%s' for --map-users", optarg);

			start = end + 1;

			id_count = strtoul(start, &end, 10);
			if (*end != '\0')
				fail_printf("c Invalid value '%s' for --map-users", optarg);

			break;
		}

		case 'r':
			freep(&dest);

			dest = realpath(optarg, NULL);
			if (dest == NULL) sysf_printf("realpath()");
			break;

		case 'c':
			freep(&change);

			change = strdup(optarg);
			break;

		case 'w':
			is_volatile = 1;
			break;

		case 'g':
			cgroup_add(&cgroups, optarg);
			break;

		case 'd':
			detach = true;
			break;

		case 'a': {
			char *end = NULL;
			pid = strtol(optarg, &end, 10);
			if (*end != '\0')
				fail_printf("Invalid value '%s' for --attach", optarg);
			break;
		}

		case 's': {
			validate_optlist("--setenv", optarg);

			if (env != NULL) {
				char *tmp = env;

			rc = asprintf(&env, "%s,%s", env, optarg);
				if (rc < 0) fail_printf("OOM");

				freep(&tmp);
			} else {
				env = strdup(optarg);
			}

			break;
		}

		case 'k':
			keepenv = true;
			break;

		case 't':
			freep(&hname);

			hname = strdup(optarg);
			break;

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

		process_pty(master_fd);
		return 0;
	}

	open_master_pty(&master_fd, &master);

	if (detach)
		do_daemonize();

	uid = getuid();

	sync_init(sync);

	pid = do_clone(&clone_flags);

	if (pid == 0) {
		closep(&master_fd);

		rc = prctl(PR_SET_PDEATHSIG, SIGKILL);
		if (rc < 0) sysf_printf("prctl(PR_SET_PDEATHSIG)");

		open_slave_pty(master);

		rc = setsid();
		if (rc < 0) sysf_printf("setsid()");

		sync_barrier_parent(sync, SYNC_START);

		sync_close(sync);

		setup_user(user);

		if (hname != NULL) {
			rc = sethostname(hname, strlen(hname));
			if (rc < 0) sysf_printf("sethostname()");
		}

		setup_mount(mounts, dest, is_volatile);

		if (dest != NULL) {
			setup_nodes(dest);

			setup_ptmx(dest);

			setup_symlinks(dest);

			setup_console(dest, master);

			do_chroot(dest);
		}

		if (clone_flags & CLONE_NEWNET)
			config_netif();

		umask(0022);

		/* TODO: drop capabilities */

		if (change != NULL) {
			rc = chdir(change);
			if (rc < 0) sysf_printf("chdir()");
		}

		if (dest != NULL) {
			char *term = getenv("TERM");

			if (!keepenv)
				clearenv();

			setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin", 1);
			setenv("USER", user, 1);
			setenv("LOGNAME", user, 1);
			setenv("TERM", term, 1);
		}

		if (env != NULL) {
			size_t i, c;

			_free_ char **vars = NULL;

			_free_ char *tmp = strdup(env);
			if (tmp == NULL) fail_printf("OOM");

			c = split_str(tmp, &vars, ",");

			for (i = 0; i < c; i++) {
				rc = putenv(strdup(vars[i]));
				if (rc != 0) sysf_printf("putenv()");
			}
		}

		setenv("container", "pflask", 1);

		if (argc > optind)
			rc = execvpe(argv[optind], argv + optind, environ);
		else
			rc = execle("/bin/bash", "-bash", NULL, environ);

		if (rc < 0) sysf_printf("exec()");
	}

	sync_wait_child(sync, SYNC_START);

	setup_cgroup(cgroups, pid);

	setup_netif(netifs, pid);

#ifdef HAVE_DBUS
	register_machine(pid, dest != NULL ? dest : "");
#endif

	if (clone_flags & CLONE_NEWUSER) {
		if (id_count == 0) {
			uid_t pw_uid, pw_gid;

			enable_setgroups(false, pid);

			get_uid_gid(user, &pw_uid, &pw_gid);

			cont_id = pw_uid;
			host_id = uid;
			id_count = 1;
		}

		map_users('u', cont_id, host_id, id_count, pid);
		map_users('g', cont_id, host_id, id_count, pid);
	}

	sync_wake_child(sync, SYNC_DONE);

	sync_close(sync);

	if (detach)
		serve_pty(master_fd);
	else
		process_pty(master_fd);

	kill(pid, SIGKILL);

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

	sync_close(sync);

	clean_cgroup(cgroups);

	return status.si_status;
}

static size_t validate_optlist(const char *name, const char *opts) {
	size_t i, c;
	_free_ char **vars = NULL;

	_free_ char *tmp = strdup(opts);
	if (tmp == NULL) fail_printf("OOM");

	c = split_str(tmp, &vars, ",");
	if (c == 0) fail_printf("Invalid value '%s' for %s", opts, name);

	for (i = 0; i < c; i++) {
		if (vars[i] == '\0')
			fail_printf("Invalid value '%s' for %s", opts, name);
	}

	return c;
}

static void do_daemonize(void) {
	int rc;

	openlog("pflask", LOG_NDELAY | LOG_PID, LOG_DAEMON);
	use_syslog = 1;

	rc = daemon(0, 0);
	if (rc < 0) sysf_printf("daemon()");
}

static void do_chroot(const char *dest) {
	int rc;

	rc = chdir(dest);
	if (rc < 0) sysf_printf("chdir()");

	rc = chroot(".");
	if (rc < 0) sysf_printf("chroot()");

	rc = chdir("/");
	if (rc < 0) sysf_printf("chdir(/)");
}

static pid_t do_clone(int *flags) {
	pid_t pid;

	*flags |= SIGCHLD;

	pid = syscall(__NR_clone, *flags, NULL);
	if (pid < 0) {
		if (errno == EINVAL) {
			*flags &= ~(CLONE_NEWUSER);
			pid = syscall(__NR_clone, *flags, NULL);
		}
	}

	if (pid < 0) sysf_printf("clone()");

	return pid;
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
	CMD_HELP("--map-users", "-e",
		"Map container users to host users");

	CMD_HELP("--chroot",  "-r",
		"Use the specified directory as root inside the container");
	CMD_HELP("--chdir", "-c",
		"Change to the specified directory inside the container");
	CMD_HELP("--volatile", "-w", "Discard changes to /");

	CMD_HELP("--cgroup", "-g",
		"Create a new cgroup and move the container inside it");

	CMD_HELP("--detach", "-d", "Detach from terminal");
	CMD_HELP("--attach", "-a", "Attach to the specified detached process");

	CMD_HELP("--setenv", "-s", "Set additional environment variables");
	CMD_HELP("--keepenv", "-k", "Do not clear environment");

	CMD_HELP("--hostname", "-t", "Set the container hostname");

	puts("");

	CMD_HELP("--no-userns",  "-U", "Disable user namespace support");
	CMD_HELP("--no-mountns", "-M", "Disable mount namespace support");
	CMD_HELP("--no-netns",   "-N", "Disable net namespace support");
	CMD_HELP("--no-ipcns",   "-I", "Disable IPC namespace support");
	CMD_HELP("--no-utsns",   "-H", "Disable UTS namespace support");
	CMD_HELP("--no-pidns",   "-P", "Disable PID namespace support");

	puts("");
}
