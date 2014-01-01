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
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>

#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/signalfd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "printf.h"
#include "util.h"

#define SOCKET_PATH "@/com/github/ghedo/pflask/%u"

static struct termios stdin_attr;
static struct winsize stdin_ws;

static int recv_fd(int sock);
static void send_fd(int sock, int fd);

void open_master_pty(int *master_fd, char **master_name) {
	int rc;

	rc = tcgetattr(STDIN_FILENO, &stdin_attr);
	if (rc < 0) sysf_printf("tcgetattr()");

	rc = ioctl(STDIN_FILENO, TIOCGWINSZ, &stdin_ws);
	if (rc < 0) sysf_printf("ioctl(TIOCGWINSZ)");

	*master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_NDELAY);
	if (*master_fd < 0) sysf_printf("posix_openpt()");

	*master_name = ptsname(*master_fd);
	if (*master_name == NULL) sysf_printf("ptsname()");

	rc = unlockpt(*master_fd);
	if (rc < 0) sysf_printf("unlckpt()");
}

void open_slave_pty(char *master_name) {
	int rc;
	_close_ int slave_fd = -1;

	slave_fd = open(master_name, O_RDWR, 0);
	if (slave_fd < 0) sysf_printf("open()");

	if (!isatty(slave_fd)) fail_printf("Not a TTY");

	rc = dup2(slave_fd, STDIN_FILENO);
	if (rc < 0) sysf_printf("dup2(STDIN)");

	rc = dup2(slave_fd, STDOUT_FILENO);
	if (rc < 0) sysf_printf("dup2(STDOUT)");

	rc = dup2(slave_fd, STDERR_FILENO);
	if (rc < 0) sysf_printf("dup2(STDERR)");

	rc = tcsetattr(slave_fd, TCSANOW, &stdin_attr);
	if (rc < 0) sysf_printf("tcsetattr()");

	rc = ioctl(slave_fd, TIOCSWINSZ, &stdin_ws);
	if (rc < 0) sysf_printf("ioctl(TIOCWINSZ)");
}

void process_pty(int master_fd) {
	int rc;

	fd_set rfds;

	sigset_t mask;

	_close_ int signal_fd = -1;

	struct termios raw_attr;

	int line_max = sysconf(_SC_LINE_MAX);
	if (line_max < 0) sysf_printf("sysconf()");

	memcpy(&raw_attr, &stdin_attr, sizeof(stdin_attr));

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGWINCH);

	rc = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (rc < 0) sysf_printf("sigprocmask()");

	signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	if (signal_fd < 0) sysf_printf("signalfd()");

	rc = tcgetattr(STDIN_FILENO, &stdin_attr);
	if (rc < 0) sysf_printf("tcgetattr()");

	cfmakeraw(&raw_attr);
	raw_attr.c_lflag &= ~ECHO;

	rc = tcsetattr(STDIN_FILENO, TCSANOW, &raw_attr);
	if (rc < 0) sysf_printf("tcsetattr()");

	FD_ZERO(&rfds);

	FD_SET(STDIN_FILENO, &rfds);
	FD_SET(master_fd, &rfds);
	FD_SET(signal_fd, &rfds);

	while ((rc = select(signal_fd + 1, &rfds, NULL, NULL, NULL)) > 0) {
		char buf[line_max];

		if (FD_ISSET(STDIN_FILENO, &rfds)) {
			char *p;

			int rc = read(STDIN_FILENO, buf, line_max);

			if (rc == 0)
				goto finish;
			else if (rc < 0)
				goto finish;

			rc = write(master_fd, buf, rc);
			if (rc < 0) sysf_printf("write()");

			for (p = buf; p < buf + rc; p++) {
				/* ^@ */
				if (*p == '\0')
					goto finish;
			}
		}

		if (FD_ISSET(master_fd, &rfds)) {
			rc = read(master_fd, buf, line_max);

			if (rc == 0)
				goto finish;
			else if (rc < 0)
				goto finish;

			rc = write(STDOUT_FILENO, buf, rc);
			if (rc < 0) sysf_printf("write()");
		}

		if (FD_ISSET(signal_fd, &rfds)) {
			struct signalfd_siginfo fdsi;

			rc = read(signal_fd, &fdsi, sizeof(fdsi));
			if (rc != sizeof(fdsi)) sysf_printf("read()");

			switch (fdsi.ssi_signo) {
				case SIGWINCH: {
					struct winsize ws;

					rc = ioctl(STDIN_FILENO, TIOCGWINSZ, &ws);
					if (rc < 0) sysf_printf("ioctl()");

					rc = ioctl(master_fd, TIOCSWINSZ, &ws);
					if (rc < 0) sysf_printf("ioctl()");

					break;
				}

				case SIGINT:
				case SIGTERM:
				case SIGCHLD:
					goto finish;
			}
		}

		FD_SET(STDIN_FILENO, &rfds);
		FD_SET(master_fd, &rfds);
		FD_SET(signal_fd, &rfds);
	}
	if (rc < 0) sysf_printf("select()");

finish:
	rc = tcsetattr(STDIN_FILENO, TCSANOW, &stdin_attr);
	if (rc < 0) sysf_printf("tcsetattr()");
}

void serve_pty(int fd) {
	int rc;

	pid_t pid;

	fd_set rfds;

	sigset_t mask;

	_close_ int sock = -1;
	_close_ int signal_fd = -1;

	_free_ char *path = NULL;

	struct sockaddr_un  servaddr_un;

	pid = getpid();

	memset(&servaddr_un, 0, sizeof(struct sockaddr_un));

	rc = asprintf(&path, SOCKET_PATH, pid);
	if (rc < 0) fail_printf("OOM");

	servaddr_un.sun_family  = AF_UNIX;
	strcpy(servaddr_un.sun_path, path);

	servaddr_un.sun_path[0] = '\0';

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) sysf_printf("socket()");

	rc = bind(sock, (struct sockaddr *) &servaddr_un, sizeof(struct sockaddr_un));
	if (rc < 0) sysf_printf("bind()");

	rc = listen(sock, 1);
	if (rc < 0) sysf_printf("listen()");

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);

	rc = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (rc < 0) sysf_printf("sigprocmask()");

	signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	if (signal_fd < 0) sysf_printf("signalfd()");

	FD_ZERO(&rfds);

	FD_SET(sock, &rfds);
	FD_SET(signal_fd, &rfds);

	while ((rc = select(signal_fd + 1, &rfds, NULL, NULL, NULL)) > 0) {
		if (FD_ISSET(sock, &rfds)) {
			socklen_t len;
			struct ucred ucred;

			_close_ int send_sock = -1;

			send_sock = accept(sock, (struct sockaddr *) NULL,
									NULL);
			if (send_sock < 0) sysf_printf("accept()");

			len = sizeof(struct ucred);
			rc = getsockopt(send_sock, SOL_SOCKET, SO_PEERCRED,
								&ucred, &len);
			if (rc < 0) sysf_printf("getsockopt(SO_PEERCRED)");

			if (ucred.uid == geteuid())
				send_fd(send_sock, fd);
			else
				send_fd(send_sock, -1);
		}

		if (FD_ISSET(signal_fd, &rfds)) {
			struct signalfd_siginfo fdsi;

			rc = read(signal_fd, &fdsi, sizeof(fdsi));
			if (rc != sizeof(fdsi)) sysf_printf("read()");

			switch (fdsi.ssi_signo) {
				case SIGINT:
				case SIGTERM:
				case SIGCHLD:
					return;
			}
		}

		FD_SET(sock, &rfds);
		FD_SET(signal_fd, &rfds);
	}
}

int recv_pty(pid_t pid) {
	int rc;
	int sock;

	_free_ char *path = NULL;

	struct sockaddr_un servaddr_un;

	rc = asprintf(&path, SOCKET_PATH, pid);
	if (rc < 0) fail_printf("OOM");

	memset(&servaddr_un, 0, sizeof(struct sockaddr_un));

	servaddr_un.sun_family = AF_UNIX;
	strcpy(servaddr_un.sun_path, path);

	servaddr_un.sun_path[0] = '\0';

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) sysf_printf("socket()");

	rc = connect(sock, (struct sockaddr *) &servaddr_un, sizeof(struct sockaddr_un));
	if (rc < 0) sysf_printf("connect()");

	return recv_fd(sock);
}

static void send_fd(int sock, int fd) {
	int rc;

	union {
		struct cmsghdr cmsg;
		char           control[CMSG_SPACE(sizeof(int))];
	} msg_control;

	struct iovec iov = {
		.iov_base = "x",
		.iov_len  = 1
	};

	struct msghdr msg = {
		.msg_name    = NULL,
		.msg_namelen = 0,
		.msg_iov     = &iov,
		.msg_iovlen  = 1,
		.msg_flags   = 0,

		.msg_control    = &msg_control,
		.msg_controllen = sizeof(msg_control)
	};

	struct cmsghdr *cmsg;

	cmsg = CMSG_FIRSTHDR(&msg);

	cmsg -> cmsg_len   = CMSG_LEN(sizeof(int));
	cmsg -> cmsg_level = SOL_SOCKET;
	cmsg -> cmsg_type  = SCM_RIGHTS;

	memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

	rc = sendmsg(sock, &msg, 0);
	if (rc < 0) sysf_printf("sendmsg()");
}

static int recv_fd(int sock) {
	int rc;

	union {
		struct cmsghdr cmsg;
		char           control[CMSG_SPACE(sizeof(int))];
	} msg_control;

	struct cmsghdr *cmsg;
	char            buf[192];

	struct iovec iov = {
		.iov_base = buf,
		.iov_len  = sizeof(buf)
	};

	struct msghdr msg = {
		.msg_name    = NULL,
		.msg_namelen = 0,
		.msg_iov     = &iov,
		.msg_iovlen  = 1,
		.msg_flags   = 0,

		.msg_control    = &msg_control,
		.msg_controllen = sizeof(msg_control)
	};

	rc = recvmsg(sock, &msg, 0);
	if (rc < 0) sysf_printf("recvmsg()");

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		int fd;

		if (cmsg -> cmsg_len   != CMSG_LEN(sizeof(int)) ||
		    cmsg -> cmsg_level != SOL_SOCKET  ||
		    cmsg -> cmsg_type  != SCM_RIGHTS)
			continue;

		fd = *((int *) CMSG_DATA(cmsg));
		return fd;
	}

	return -1;
}
