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

#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "printf.h"
#include "util.h"

int sync_init(int fd[2]) {
    int rc;

    rc = socketpair(AF_LOCAL, SOCK_STREAM, 0, fd);
    sys_fail_if(rc < 0, "Error creating socket pair");

    rc = fcntl(fd[0], F_SETFD, FD_CLOEXEC);
    sys_fail_if(rc < 0, "Error setting FD_CLOEXEC");

    return 0;
}

static int sync_wait(int fd, int seq) {
    int rc;
    int sync = -1;

    rc = read(fd, &sync, sizeof(sync));
    sys_fail_if(rc < 0, "Error reading from socket");

    if (!rc)
        return 0;

    if (sync != seq)
        fail_printf("Invalid sync sequence: %d != %d", seq, sync);

    return 0;
}

int sync_wait_child(int fd[2], int seq) {
    return sync_wait(fd[1], seq);
}

int sync_wait_parent(int fd[2], int seq) {
    return sync_wait(fd[0], seq);
}

static int sync_wake(int fd, int seq) {
    int rc;

    rc = write(fd, &seq, sizeof(seq));
    sys_fail_if(rc < 0, "Error waking process");

    return 0;
}

int sync_wake_child(int fd[2], int seq) {
    return sync_wake(fd[1], seq);
}

int sync_wake_parent(int fd[2], int seq) {
    return sync_wake(fd[0], seq);
}

static int sync_barrier(int fd, int seq) {
    if (sync_wake(fd, seq))
        return -1;

    return sync_wait(fd, seq + 1);
}

int sync_barrier_child(int fd[2], int seq) {
    return sync_barrier(fd[1], seq);
}

int sync_barrier_parent(int fd[2], int seq) {
    return sync_barrier(fd[0], seq);
}

void sync_close_child(int fd[2]) {
    if (fd[0] != -1)
        closep(&fd[0]);
}

void sync_close_parent(int fd[2]) {
    if (fd[1] != -1)
        closep(&fd[1]);
}

void sync_close(int fd[2]) {
    sync_close_child(fd);
    sync_close_parent(fd);
}
