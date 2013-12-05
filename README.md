pflask
======

![Travis CI](https://secure.travis-ci.org/ghedo/pflask.png)

**pflask** is a simple tool for creating Linux namespace containers. It can be
used for running a command or even booting an OS inside an isolated container,
created with the help of Linux namespaces. It is similar in functionality to
`chroot(8)`, altough pflask provides better isolation thanks to the use of
namespaces. 

Compared to [LXC] [LXC], pflask is easier to use since it doesn't require any
pre-configuration (all the options can be passed via the command-line), but LXC
is better suited for production, or generally security-sensitive environments.

Compared to [systemd-nspawn] [systemd], pflask doesn't require the use of
systemd on the host system and provides additional features such as a more
comprehensive handling of mounts and network interfaces inside the container.

[LXC]: http://linuxcontainers.org
[systemd]: http://www.freedesktop.org/software/systemd/man/systemd-nspawn.html

## FEATURES

### User namespace

When the host system allows it, pflask creates a new user namespace inside the
container, and automatically maps the user running pflask to the root user
inside the container. This means that a user could create and have full root
privileges inside a container, while having none on the host system.

Note that this has been the cause of security vulnerabilities in the past, so
that most OS vendors (reasonably) decided to either disable user namespace
support altogether, or restrict the functionality to root.

pflask can disable the relevant functionality when it detects that support for
user namespaces is not available.

### Mount namespace

By default, pflask creates a new mount namespace inside the container, so that
filesystems mounted inside it won't affect the host system. pflask can also be
told to create new mount points before the execution of the supplied command, 
by using the `--mount` option. Supported mount point types are:

 * `bind` -- bind mount a directory/file to another directory/file
 * `aufs` -- stack a directory on top of another directory using AuFS
 * `loop` -- mount a loop device (TODO)
 * `tmp`  -- mount a tmpfs on a directory

### Network namespace

When supplied the `--netif` option, pflask will create a new network namespace
and move/rename the supplied network interface inside the container.

### PID, IPC and UTS namespaces

By default, pflask creates new PID, IPC and UTS namespaces inside the container,
in order to isolate processes, IPC resources and the node/domain name of the
container from the host system.

## GETTING STARTED

See the [man page](http://ghedo.github.io/pflask/) for more information.

## DEPENDENCIES

 * `linux`
 * `libnl`
 * `libnl-route`

## BUILDING

pflask is distributed as source code. Install with:

```bash
$ mkdir build && cd build
$ cmake ..
$ make
$ [sudo] make install
```

## COPYRIGHT

Copyright (C) 2013 Alessandro Ghedini <alessandro@ghedini.me>

See COPYING for the license.
