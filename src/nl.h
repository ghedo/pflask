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

struct nlmsg {
    struct nlmsghdr hdr;

    union {
        struct ifinfomsg ifi;
        struct nlmsgerr  err;
    } msg;
};

#define NLMSG_TAIL(nmsg) \
 ((struct rtattr *) (((char *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

#define NLMSG_GOOD_SIZE (sizeof(struct nlmsg) * 256)

int nl_open(void);
void nl_send(int sock, struct nlmsg *nlmsg);
void nl_recv(int sock, struct nlmsg *nlmsg);

void rtattr_append(struct nlmsg *nlmsg, int attr, void *d, size_t len);
struct rtattr *rtattr_start_nested(struct nlmsg *nlmsg, int attr);
void rtattr_end_nested(struct nlmsg *nlmsg, struct rtattr *rtattr);
