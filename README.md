pflask
======

![Travis CI](https://secure.travis-ci.org/ghedo/pflask.png)

**pflask** is a simple tool for creating Linux namespace containers. It can be
used for running a command or even booting an OS inside an isolated container,
created with the help of Linux namespaces. It is similar in functionality to
`chroot(8)`, although pflask provides better isolation thanks to the use of
namespaces. Unlike chroot and most other containerization solutions, pflask can
also be used without changing the root directory inside the container.

Compared to [LXC] [LXC], pflask is easier to use since it doesn't require any
pre-configuration (all the options can be passed via the command-line). pflask
is mostly intended for testing/building/experimenting, whereas LXC is better
suited for production environments.

Compared to [systemd-nspawn] [systemd], pflask doesn't require the use of
systemd on the host system and provides a better interface for manipulating
mount points and network interfaces inside the container.

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
 * `tmp`  -- mount a tmpfs on a directory

### Network namespace

When supplied the `--netif` option, pflask will create a new network namespace
and move/rename the supplied network interface inside the container.

### PID, IPC and UTS namespaces

By default, pflask creates new PID, IPC and UTS namespaces inside the container,
in order to isolate processes, IPC resources and the node/domain name of the
container from the host system.

## EXAMPLES

### Hide directories from an application

```bash
$ pflask --user=$USER --mount=tmp,$HOME chromium --disable-setuid-sandbox
```

This command does not require a pre-generated chroot (it will use the current
root) and will mount a tmpfs on `$HOME` so that the application (chromium in the
example) won't be able to access your precious files. Any change will be also
discarded once the process terminates. A bind mount can be used to retain the
modifications:

```bash
$ pflask --user=$USER --mount=bind,/tmp/trash,$HOME  chromium --disable-setuid-sandbox
```

All filesystem changes applied by the command will be available in /tmp/trash.

Both commands can be run without root privileges as long as user namespaces are
supported by the host system, and available to non-privileged users.

### Detach from terminal

```bash
$ pflask --user=$USER --detach /bin/bash
```

To reattach run pflask with the `--attach` option

```bash
$ pgrep pflask
29076
$ pflask --attach=29076
```

Where _29076_ is the PID of the detached pflask process. Once reattached, one
can detach again by pressing _^@_ (Ctrl + @).

### Boot the OS inside the container

```bash
$ sudo pflask --root=/path/to/container /sbin/init
```

This will simply execute the init system inside the container. It is recommended
to use systemd inside the guest system, since it can detect whether it is
run inside a container or not, and disable services accordingly.

### Disable network inside the container

```bash
$ sudo pflask --root=/path/to/container --netif /sbin/init
```

Using the `--netif` option without any argument creates a new network namespace
inside the container without adding any new interface, therefore leaving the
_lo_ interface as the only one inside the container and disabling network access
to the outside world while at the same time leaving the network on the host
system working.

### Use a private network interface inside the container

First, let's create the new network interface thet will be used inside the
container:

```bash
$ sudo ip link add link eth0 pflask-vlan0 type macvlan
```

This will create a new interface, `pflask-vlan0`, of type `macvlan` using the
`eth0` interface on the host as master. `macvlan` interfaces can be used to
give a second MAC address to a network adapter (in this case `eth0`) and make
it look like a completely different device.

Finally, create the container:

```bash
$ sudo pflask --root=/path/to/container --netif=pflask-vlan0,eth0 /sbin/init
```

This will take the `pflask-vlan0` interface previously created, move it inside
the container and rename it to `eth0`. The container will thus have what it
looks like a private `eth0` network interface that can be configured
independently from the host `eth0`. Once the container terminates, its network
interface will be destroyed as well.

Note that `macvlan` is just one of the possibilities. One could create a pair
of `veth` interfaces, move one of them inside the container and connect the
other to a bridge (e.g. an Open VSwitch bridge). Alternatively one could create
a `vxlan` interface and connect the container to a VXLAN network, etc...

### Copy-on-write filesystem

```bash
$ sudo pflask --root=/path/to/container --no-userns \
  --mount=aufs,/tmp/overlay,/path/to/container \
  /sbin/init
```

This will mount a copy-on-write filesystem on the / of the container. Any change
to files and directories will be saved in `/tmp/overlay` so that the container
root directory (`/path/to/container`) will be unaffected.

Note that this requires support for AuFS on the host system. Also, AuFS does not
(yet?) support user namespaces, so that they need to be disabled (that's what
the `--no-userns` option is for).

### Build a Debian package inside a container

First, create the chroot directory:

```bash
$ sudo mkdir -p /var/cache/pflask
$ sudo debootstrap --arch=amd64 --variant=buildd unstable /var/cache/pflask/base-unstable-amd64
```

Then retrieve the source package we want to build:

```bash
$ apt-get source somepackage
$ cd somepackage-XYX
```

Where _somepackage_ is the desired package, and _XYZ_ is the package version.

Finally build the package:

```bash
$ pflask-debuild
```

The script will take care of creating a new container, installing all the
required dependncies (inside the container), building and signing the package.

A copy-on-write filesystem is also mounted on the / of the container, so that
the same clean chroot can be re-used to build other packages.

Note that the [pflask-debuild](tools/pflask-debuild) tool is far from perfect,
and may not work in all situations.

See the [man page](http://ghedo.github.io/pflask/) for more information.

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
