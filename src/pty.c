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
#include <signal.h>

#include <fcntl.h>
#include <termios.h>

#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>

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

	if (isatty(STDIN_FILENO)) {
		rc = tcgetattr(STDIN_FILENO, &stdin_attr);
		sys_fail_if(rc < 0, "tcgetattr()");

		rc = ioctl(STDIN_FILENO, TIOCGWINSZ, &stdin_ws);
		sys_fail_if(rc < 0, "ioctl(TIOCGWINSZ)");
	}

	*master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_NDELAY);
	sys_fail_if(*master_fd < 0, "Error opening master pty");

	*master_name = ptsname(*master_fd);
	sys_fail_if(!*master_name, "ptsname()");

	rc = unlockpt(*master_fd);
	sys_fail_if(rc < 0, "Error unlocking master pty");
}

void open_slave_pty(const char *master_name) {
	int rc;

	_close_ int slave_fd = -1;

	slave_fd = open(master_name, O_RDWR, 0);
	sys_fail_if(slave_fd < 0, "Error opening slave pty");

	fail_if(!isatty(slave_fd), "Not a TTY");

	rc = dup2(slave_fd, STDIN_FILENO);
	sys_fail_if(rc < 0, "dup2(STDIN)");

	rc = dup2(slave_fd, STDOUT_FILENO);
	sys_fail_if(rc < 0, "dup2(STDOUT)");

	rc = dup2(slave_fd, STDERR_FILENO);
	sys_fail_if(rc < 0, "dup2(STDERR)");

	if (isatty(STDIN_FILENO)) {
		rc = tcsetattr(slave_fd, TCSANOW, &stdin_attr);
		sys_fail_if(rc < 0, "tcsetattr()");

		rc = ioctl(slave_fd, TIOCSWINSZ, &stdin_ws);
		sys_fail_if(rc < 0, "ioctl(TIOCWINSZ)");
	}
}

void process_pty(int master_fd) {
	int rc;

	sigset_t mask;

	_close_ int epoll_fd  = -1;
	_close_ int signal_fd = -1;

	struct termios raw_attr;

	struct epoll_event stdin_ev, master_ev, signal_ev, events[3];

	int line_max = sysconf(_SC_LINE_MAX);
	sys_fail_if(line_max < 0, "sysconf()");

	memcpy(&raw_attr, &stdin_attr, sizeof(stdin_attr));

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGWINCH);
	sigaddset(&mask, SIGRTMIN + 4);

	rc = sigprocmask(SIG_BLOCK, &mask, NULL);
	sys_fail_if(rc < 0, "sigprocmask()");

	signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	sys_fail_if(signal_fd < 0, "signalfd()");

	rc = tcgetattr(STDIN_FILENO, &stdin_attr);
	sys_fail_if(rc < 0, "tcgetattr()");

	cfmakeraw(&raw_attr);
	raw_attr.c_lflag &= ~ECHO;

	rc = tcsetattr(STDIN_FILENO, TCSANOW, &raw_attr);
	sys_fail_if(rc < 0, "tcsetattr()");

	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	sys_fail_if(epoll_fd < 0, "epoll_create1()");

	stdin_ev.events = EPOLLIN; stdin_ev.data.fd = STDIN_FILENO;
	rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stdin_ev.data.fd, &stdin_ev);
	sys_fail_if(rc < 0, "epoll_ctl(STDIN_FILENO)");

	master_ev.events = EPOLLIN; master_ev.data.fd = master_fd;
	rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, master_ev.data.fd, &master_ev);
	sys_fail_if(rc < 0, "epoll_ctl(master_fd)");

	signal_ev.events = EPOLLIN; signal_ev.data.fd = signal_fd;
	rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_ev.data.fd, &signal_ev);
	sys_fail_if(rc < 0, "epoll_ctl(signal_fd)");

	while (1) {
		char buf[line_max];

		rc = epoll_wait(epoll_fd, events, 1, -1);
		sys_fail_if(rc < 0, "epoll_wait()");

		if (events[0].data.fd == STDIN_FILENO) {
			char *p;

			int rc = read(STDIN_FILENO, buf, line_max);

			if (!rc)
				goto done;
			else if (rc < 0)
				goto done;

			rc = write(master_fd, buf, rc);
			sys_fail_if(rc < 0, "write()");

			for (p = buf; p < buf + rc; p++) {
				if (*p == '\0')
					goto done;
			}
		}

		if (events[0].data.fd == master_fd) {
			rc = read(master_fd, buf, line_max);

			if (!rc)
				goto done;
			else if (rc < 0)
				goto done;

			rc = write(STDOUT_FILENO, buf, rc);
			sys_fail_if(rc < 0, "write()");
		}

		if (events[0].data.fd == signal_fd) {
			struct signalfd_siginfo fdsi;

			rc = read(signal_fd, &fdsi, sizeof(fdsi));
			sys_fail_if(rc != sizeof(fdsi), "read()");

			switch (fdsi.ssi_signo) {
			case SIGWINCH: {
				struct winsize ws;

				rc = ioctl(STDIN_FILENO,TIOCGWINSZ,&ws);
				sys_fail_if(rc < 0, "ioctl()");

				rc = ioctl(master_fd, TIOCSWINSZ, &ws);
				sys_fail_if(rc < 0, "ioctl()");

				break;
			}

			case SIGINT:
			case SIGTERM:
			case SIGCHLD:
				goto done;
			}

			if (fdsi.ssi_signo == (unsigned int) SIGRTMIN + 4)
				goto done;
		}
	}

done:
	rc = tcsetattr(STDIN_FILENO, TCSANOW, &stdin_attr);
	sys_fail_if(rc < 0, "tcsetattr()");
}

void serve_pty(int fd) {
	int rc;

	pid_t pid;

	sigset_t mask;

	_close_ int sock = -1;
	_close_ int epoll_fd  = -1;
	_close_ int signal_fd = -1;

	_free_ char *path = NULL;

	struct epoll_event sock_ev, signal_ev, events[3];

	struct sockaddr_un servaddr_un;

	pid = getpid();

	memset(&servaddr_un, 0, sizeof(struct sockaddr_un));

	rc = asprintf(&path, SOCKET_PATH, pid);
	fail_if(rc < 0, "OOM");

	if ((size_t) rc >= sizeof(servaddr_un.sun_path))
		fail_printf("Socket path too long");

	memset(&servaddr_un, 0, sizeof(struct sockaddr_un));

	servaddr_un.sun_family  = AF_UNIX;

	snprintf(servaddr_un.sun_path, sizeof(servaddr_un.sun_path), "%s", path);
	servaddr_un.sun_path[0] = '\0';

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	sys_fail_if(sock < 0, "socket()");

	rc = bind(sock, (struct sockaddr *) &servaddr_un, sizeof(struct sockaddr_un));
	sys_fail_if(rc < 0, "bind()");

	rc = listen(sock, 1);
	sys_fail_if(rc < 0, "listen()");

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGRTMIN + 4);

	rc = sigprocmask(SIG_BLOCK, &mask, NULL);
	sys_fail_if(rc < 0, "sigprocmask()");

	signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
	sys_fail_if(signal_fd < 0, "signalfd()");

	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	sys_fail_if(epoll_fd < 0, "epoll_create1()");

	sock_ev.events = EPOLLIN; sock_ev.data.fd = sock;
	rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_ev.data.fd, &sock_ev);
	sys_fail_if(rc < 0, "epoll_ctl(STDIN_FILENO)");

	signal_ev.events = EPOLLIN; signal_ev.data.fd = signal_fd;
	rc = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, signal_ev.data.fd, &signal_ev);
	sys_fail_if(rc < 0, "epoll_ctl(signal_fd)");

	while (1) {
		rc = epoll_wait(epoll_fd, events, 1, -1);
		sys_fail_if(rc < 0, "epoll_wait()");

		if (events[0].data.fd == sock) {
			socklen_t len;
			struct ucred ucred;

			_close_ int send_sock = -1;

			send_sock = accept(sock, (struct sockaddr *) NULL,NULL);
			sys_fail_if(send_sock < 0, "accept()");

			len = sizeof(struct ucred);
			rc = getsockopt(send_sock, SOL_SOCKET, SO_PEERCRED,
			                &ucred, &len);
			sys_fail_if(rc < 0, "getsockopt(SO_PEERCRED)");

			if (ucred.uid == geteuid())
				send_fd(send_sock, fd);
		}

		if (events[0].data.fd == signal_fd) {
			struct signalfd_siginfo fdsi;

			rc = read(signal_fd, &fdsi, sizeof(fdsi));
			sys_fail_if(rc != sizeof(fdsi), "read()");

			switch (fdsi.ssi_signo) {
			case SIGINT:
			case SIGTERM:
			case SIGCHLD:
				return;
			}

			if (fdsi.ssi_signo == (unsigned int) SIGRTMIN + 4)
				return;
		}
	}
}

int recv_pty(pid_t pid) {
	int rc;

	_close_ int sock = -1;

	_free_ char *path = NULL;

	struct sockaddr_un servaddr_un;

	rc = asprintf(&path, SOCKET_PATH, pid);
	fail_if(rc < 0, "OOM");

	if ((size_t) rc >= sizeof(servaddr_un.sun_path))
		fail_printf("Socket path too long");

	memset(&servaddr_un, 0, sizeof(struct sockaddr_un));

	servaddr_un.sun_family = AF_UNIX;

	snprintf(servaddr_un.sun_path, sizeof(servaddr_un.sun_path), "%s", path);
	servaddr_un.sun_path[0] = '\0';

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	sys_fail_if(sock < 0, "socket()");

	rc = connect(sock, (struct sockaddr *) &servaddr_un, sizeof(struct sockaddr_un));
	sys_fail_if(rc < 0, "connect()");

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

	cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type  = SCM_RIGHTS;

	memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

	rc = sendmsg(sock, &msg, 0);
	sys_fail_if(rc < 0, "sendmsg()");
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
	sys_fail_if(rc < 0, "recvmsg()");

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		int fd;

		if (cmsg->cmsg_len   != CMSG_LEN(sizeof(int)) ||
		    cmsg->cmsg_level != SOL_SOCKET  ||
		    cmsg->cmsg_type  != SCM_RIGHTS)
			continue;

		fd = *((int *) CMSG_DATA(cmsg));
		return fd;
	}

	return -1;
}
