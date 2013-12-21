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
#include <string.h>

#include <sys/uio.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>

#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "netif.h"
#include "printf.h"
#include "util.h"

#define NLMSG_TAIL(nmsg) \
 ((struct rtattr *) (((unsigned char *) (nmsg)) + NLMSG_ALIGN((nmsg) -> nlmsg_len)))

typedef enum NETIF_TYPE {
	MOVE,
	MACVLAN
} netif_type;

typedef struct NETIF_LIST {
	char *dev;
	char *name;

	enum NETIF_TYPE type;

	struct NETIF_LIST *next;
} netif_list;

struct nlmsg {
	struct nlmsghdr hdr;
	union {
		struct ifinfomsg ifi;
		struct nlmsgerr  err;
	} msg;
};

static netif_list *netifs = NULL;

static void move_and_rename_if(int sock, pid_t pid, int i, char *new_name);
static void create_macvlan(int sock, int master, char *name);

static void rtattr_append(struct nlmsg *nlmsg, int attr, void *d, size_t len);
static struct rtattr *rtattr_start_nested(struct nlmsg *nlmsg, int attr);
static void rtattr_end_nested(struct nlmsg *nlmsg, struct rtattr *rtattr);

static void nl_send(int sock, struct nlmsg *nlmsg);
static void nl_recv(int sock, struct nlmsg *nlmsg);

void add_netif(netif_type type, char *dev, char *name) {
	netif_list *nif = malloc(sizeof(netif_list));
	if (nif == NULL) fail_printf("OOM");

	nif -> dev  = strdup(dev);
	nif -> name = strdup(name);
	nif -> type = type;

	nif -> next  = NULL;

	if (netifs)
		nif -> next = netifs;

	netifs = nif;
}

void add_netif_from_spec(char *spec) {
	_free_ char *tmp = NULL;
	_free_ char **opts = NULL;

	if (spec == NULL)
		return;

	tmp = strdup(spec);
	if (tmp == NULL) fail_printf("OOM");

	size_t c = split_str(tmp, &opts, ",");
	if (c == 0) fail_printf("Invalid netif spec '%s'", spec);

	if (if_nametoindex(opts[0])) {
		if (c < 2) fail_printf("Invalid netif spec '%s'", spec);
		add_netif(MOVE, opts[0], opts[1]);
	} else if (strncmp(opts[0], "macvlan", 8) == 0) {
		if (c < 3) fail_printf("Invalid netif spec '%s'", spec);
		add_netif(MACVLAN, opts[1], opts[2]);
	} else
		fail_printf("Invalid netif spec '%s'", spec);
}

void do_netif(pid_t pid) {
	int rc;
	_close_ int sock = -1;

	netif_list *i = NULL;
	struct sockaddr_nl addr;

	sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (sock < 0) sysf_printf("socket()");

	addr.nl_family = AF_NETLINK;
	addr.nl_pad    = 0;
	addr.nl_pid    = getpid();
	addr.nl_groups = 0;

	rc = bind(sock, (struct sockaddr *) &addr, sizeof(struct sockaddr_nl));
	if (rc < 0) sysf_printf("bind()");

	while (netifs) {
		netif_list *next = netifs -> next;
		netifs -> next = i;
		i = netifs;
		netifs = next;
	}

	while (i != NULL) {
		unsigned int if_index = if_nametoindex(i -> dev);
		if (if_index == 0) sysf_printf("Error searching for '%s'",
								i -> dev);

		switch (i -> type) {
			case MACVLAN: {
				_free_ char *name = NULL;

				rc = asprintf(&name, "pflask-%d", pid);
				if (rc < 0) fail_printf("OOM");

				create_macvlan(sock, if_index, name);

				if_index = if_nametoindex(name);
				break;
			}

			case MOVE:
				break;
		}

		move_and_rename_if(sock, pid, if_index, i -> name);

		i = i -> next;
	}
}

static void move_and_rename_if(int sock, pid_t pid, int if_index, char *new_name) {
	_free_ struct nlmsg *req = malloc(4096);

	req -> hdr.nlmsg_seq   = 1;
	req -> hdr.nlmsg_type  = RTM_NEWLINK;
	req -> hdr.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req -> hdr.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;

	req -> msg.ifi.ifi_family  = AF_UNSPEC;
	req -> msg.ifi.ifi_index   = if_index;

	rtattr_append(req, IFLA_NET_NS_PID, &pid, sizeof(pid));
	rtattr_append(req, IFLA_IFNAME, new_name, strlen(new_name) + 1);

	nl_send(sock, req);
	nl_recv(sock, req);

	if (req -> hdr.nlmsg_type == NLMSG_ERROR) {
		if (req -> msg.err.error < 0)
			fail_printf("Error sending netlink request: %s",
					strerror(-req -> msg.err.error));
	}
}

static void create_macvlan(int sock, int master, char *name) {
	struct rtattr *nested = NULL;

	_free_ struct nlmsg *req = malloc(4096);

	req -> hdr.nlmsg_seq   = 1;
	req -> hdr.nlmsg_type  = RTM_NEWLINK;
	req -> hdr.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	req -> hdr.nlmsg_flags = NLM_F_REQUEST |
				 NLM_F_CREATE  |
				 NLM_F_EXCL    |
				 NLM_F_ACK;

	req -> msg.ifi.ifi_family  = AF_UNSPEC;

	nested = rtattr_start_nested(req, IFLA_LINKINFO);
	rtattr_append(req, IFLA_INFO_KIND, "macvlan", 8);
	rtattr_end_nested(req, nested);

	rtattr_append(req, IFLA_LINK, &master, sizeof(master));
	rtattr_append(req, IFLA_IFNAME, name, strlen(name) + 1);

	nl_send(sock, req);
	nl_recv(sock, req);

	if (req -> hdr.nlmsg_type == NLMSG_ERROR) {
		if (req -> msg.err.error < 0)
			fail_printf("Error sending netlink request: %s",
					strerror(-req -> msg.err.error));
	}
}

static void rtattr_append(struct nlmsg *nlmsg, int attr, void *d, size_t len) {
	struct rtattr *rtattr;
	size_t rtalen = RTA_LENGTH(len);

	rtattr = NLMSG_TAIL(&nlmsg -> hdr);
	rtattr -> rta_type = attr;
	rtattr -> rta_len  = rtalen;

	memcpy(RTA_DATA(rtattr), d, len);

	nlmsg -> hdr.nlmsg_len = NLMSG_ALIGN(nlmsg -> hdr.nlmsg_len) +
				 RTA_ALIGN(rtalen);
}

static struct rtattr *rtattr_start_nested(struct nlmsg *nlmsg, int attr) {
	struct rtattr *rtattr = NLMSG_TAIL(&nlmsg -> hdr);

	rtattr_append(nlmsg, attr, NULL, 0);

	return rtattr;
}

static void rtattr_end_nested(struct nlmsg *nlmsg, struct rtattr *rtattr) {
	rtattr -> rta_len = (void *) NLMSG_TAIL(&nlmsg -> hdr) - (void *) rtattr;
}

static void nl_send(int sock, struct nlmsg *nlmsg) {
	int rc;
	struct sockaddr_nl addr;

	struct iovec iov = {
		.iov_base = (void *) nlmsg,
		.iov_len  = nlmsg -> hdr.nlmsg_len
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

static void nl_recv(int sock, struct nlmsg *nlmsg) {
	int rc;
	struct sockaddr_nl addr;

	struct iovec iov = {
		.iov_base = (void *) nlmsg,
		.iov_len  = nlmsg -> hdr.nlmsg_len
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
