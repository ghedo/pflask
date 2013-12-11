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

#include <netlink/msg.h>
#include <netlink/attr.h>
#include <netlink/errno.h>
#include <netlink/socket.h>
#include <netlink/route/link.h>

#include "netif.h"
#include "printf.h"
#include "util.h"

typedef struct NETIF_LIST {
	char *dev;
	char *name;

	struct NETIF_LIST *next;
} netif_list;

static netif_list *netifs = NULL;

void add_netif(char *dev, char *name) {
	netif_list *nif = malloc(sizeof(netif_list));
	if (nif == NULL) fail_printf("OOM");

	nif -> dev  = strdup(dev);
	nif -> name = strdup(name);

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

	if (c < 2) fail_printf("Invalid netif spec '%s'", spec);

	add_netif(opts[0], opts[1]);
}

void do_netif(pid_t pid) {
	int rc;
	netif_list *i = NULL;

	struct nl_sock  *sock;
	struct nl_cache *cache;

	sock = nl_socket_alloc();
	if (sock == NULL) fail_printf("OOM");

	rc = nl_connect(sock, NETLINK_ROUTE);
	if (rc < 0) fail_printf("Error creating netlink connection: %s",
						nl_geterror(rc));

	rc = rtnl_link_alloc_cache(sock, AF_UNSPEC, &cache);
	if (rc < 0) fail_printf("Error creating netlink cache: %s",
						nl_geterror(rc));

	while (netifs) {
		netif_list *next = netifs -> next;
		netifs -> next = i;
		i = netifs;
		netifs = next;
	}

	while (i != NULL) {
		struct nl_msg *msg;
		struct rtnl_link *new_link;

		/* TODO: roll own netlink and get rid of libnl */
		struct rtnl_link *link = rtnl_link_get_by_name(cache, i->dev);
		if (link == NULL) fail_printf("Invalid netif '%s'", i -> dev);

		new_link = rtnl_link_alloc();
		if (new_link == NULL) fail_printf("OOM");

		rtnl_link_set_name(new_link, i -> name);

		rc = rtnl_link_build_change_request(link, new_link, 0, &msg);
		if (rc < 0) fail_printf("Error creating netlink request: '%s'",
						nl_geterror(rc));

		nla_put_u32(msg, IFLA_NET_NS_PID, pid);

		rc = nl_send_auto(sock, msg);
		if (rc < 0) fail_printf("Error sending netlink request: %s",
						nl_geterror(rc));

		i = i -> next;

		rtnl_link_put(new_link);
		rtnl_link_put(link);
	}

	nl_cache_free(cache);

	nl_close(sock);
	nl_socket_free(sock);
}
