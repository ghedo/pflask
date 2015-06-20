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

#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <linux/rtnetlink.h>

#include "nl.h"
#include "printf.h"
#include "util.h"

void rtattr_append(struct nlmsg *nlmsg, int attr, void *d, size_t len) {
	struct rtattr *rtattr;
	size_t rtalen = RTA_LENGTH(len);

	rtattr = NLMSG_TAIL(&nlmsg->hdr);
	rtattr->rta_type = attr;
	rtattr->rta_len  = rtalen;

	memcpy(RTA_DATA(rtattr), d, len);

	nlmsg->hdr.nlmsg_len = NLMSG_ALIGN(nlmsg->hdr.nlmsg_len) +
	                         RTA_ALIGN(rtalen);
}

struct rtattr *rtattr_start_nested(struct nlmsg *nlmsg, int attr) {
	struct rtattr *rtattr = NLMSG_TAIL(&nlmsg->hdr);

	rtattr_append(nlmsg, attr, NULL, 0);

	return rtattr;
}

void rtattr_end_nested(struct nlmsg *nlmsg, struct rtattr *rtattr) {
	rtattr->rta_len = (char *) NLMSG_TAIL(&nlmsg->hdr) - (char *) rtattr;
}

int nl_open(void) {
	int rc;
	int sock = -1;

	struct sockaddr_nl addr;

	sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sock < 0) sysf_printf("socket()");

	addr.nl_family = AF_NETLINK;
	addr.nl_pad    = 0;
	addr.nl_pid    = getpid();
	addr.nl_groups = 0;

	rc = bind(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_nl));
	if (rc < 0) sysf_printf("bind()");

	return sock;
}

void nl_send(int sock, struct nlmsg *nlmsg) {
	int rc;
	struct sockaddr_nl addr;

	struct iovec iov = {
		.iov_base = (void *) nlmsg,
		.iov_len  = nlmsg->hdr.nlmsg_len
	};

	struct msghdr msg = {
		.msg_name    = &addr,
		.msg_namelen = sizeof(struct sockaddr_nl),
		.msg_iov     = &iov,
		.msg_iovlen  = 1
	};

	memset(&addr, 0, sizeof(struct sockaddr_nl));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid    = 0;
	addr.nl_groups = 0;

	rc = sendmsg(sock, &msg, 0);
	if (rc < 0) sysf_printf("sendmsg()");
}

void nl_recv(int sock, struct nlmsg *nlmsg) {
	int rc;
	struct sockaddr_nl addr;

	struct iovec iov = {
		.iov_base = (void *) nlmsg,
		.iov_len  = nlmsg->hdr.nlmsg_len
	};

	struct msghdr msg = {
		.msg_name    = &addr,
		.msg_namelen = sizeof(struct sockaddr_nl),
		.msg_iov     = &iov,
		.msg_iovlen  = 1
	};

	memset(&addr, 0, sizeof(struct sockaddr_nl));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid    = 0;
	addr.nl_groups = 0;

	rc = recvmsg(sock, &msg, 0);
	if (rc < 0) sysf_printf("recvmsg()");
}
