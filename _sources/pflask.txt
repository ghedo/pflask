.. _pflask(1):

pflask
======

SYNOPSIS
--------

.. program:: pflask

**pflask [options] [--] [command [args...]]**

DESCRIPTION
-----------

**pflask** is a simple tool for creating Linux namespace containers. It can be
used for running a command or even booting an OS inside an isolated container,
created with the help of Linux namespaces. It is similar in functionality to
`chroot(8)`, although pflask provides better isolation thanks to the use of
namespaces.

Additionally, while most other containerization solutions (LXC, systemd-nspawn,
...) are mostly targeted at containing whole systems, pflask can also be used to
contain single programs, without the need to create ad-hoc chroots.

OPTIONS
-------

.. option:: -m, --mount=<type>,<opts>

Create a new *type* mount point inside the container. See MOUNT_.

.. option:: -n, --netif[=<opts>]

Create a new network namespace and, optionally create/move a network interface
inside the container. See NETIF_.

.. option:: -u, --user=<user>

Run the command under the specified user.

.. option:: -e, --user-map=<map>

Map container users to host users. This is a generalization of the ``--user``
command, and can create maps over arbitrary UIDs ranges. The *map* argument
is composed of three values separated by ``:``: the first userid as seen in
the user namespace of the container, the first userid as seen on the host, and
a range indicating the number of consecutive ids to map.

Example: `--user-map=0:100000,65536`

.. option:: -r, --chroot=<dir>

Change the root directory inside the container.

.. option:: -c, --chdir=<dir>

Change the current directory inside the container.

.. option:: -w, --volatile

Discard any change to / once the container exits. This can only be used along
with ``--chroot`` and requires support for the overlay_ mount type.

.. option:: -g, --cgroup=<controller>

Create a new cgroup in the given controller and move the container inside it.

.. option:: -d, --detach

Detach from terminal.

.. option:: -a, --attach=<pid>

Attach to the *pid* detached process. Only a process with the same UID of the
detached process can attach to it. To detach again press `^@` (Ctrl + @).

.. option:: -s, --setenv=<name>=<value>[,<name>=<value> ...]

Set additional environment variables. It takes a comma-separated list of
variables of the form `name=value`. This option may be used more than once.

.. option:: -k, --keepenv

Do not clear environment (only relevant when used with `--chroot`).

.. option:: -t, --hostname

Set the container hostname.

.. option:: -U, --no-userns

Disable user namespace.

.. option:: -M, --no-mountns

Disable mount namespace.

.. option:: -N, --no-netns

Disable net namespace.

.. option:: -I, --no-ipcns

Disable IPC namespace.

.. option:: -H, --no-utsns

Disable UTS namespace.

.. option:: -P, --no-pidns

Disable PID namespace.

MOUNT
-----

pflask support the following mount point types:

bind
~~~~

It bind mounts a directory/file to another directory/file. If the `--chroot`
option is used, the destination path will be relative to the chroot.

Example: `--mount=bind,/source/path,/dest/path`

bind-ro
~~~~~~~

Same as `bind`, but make the mount point read-only. If the `--chroot` option
is used, the destination path will be relative to the chroot.

Example: `--mount=bind-ro,/source/path,/dest/path`

overlay
~~~~~~~

It stacks a directory on top of another directory using either AuFS or OverlayFS
depending on what is found at compile-time.

Note that AuFS and OverlayFS don't support user namespaces, so the `--user`
option is incompatible with this mount type unless `--no-userns` is also used.

In the following example, "/overlay/path" is stacked on top of "/dest/path". The
"/overlay/work" directory needs to be an empty directory on the same filesystem
as "/overlay/path".

Example: `--mount=overlay,/overlay/path,/dest/path,/overlay/work`

tmp
~~~

It mounts a tmpfs on a directory.

Example: `--mount=tmp,/dest/path`

NETIF
-----

When the `--netif` option is used, pflask will create a new network namespace
inside the container. If the argument is set, the following actions will be
taken:

move and rename
~~~~~~~~~~~~~~~

`--netif=<dev>,<name>`

If the *dev* option is an existing network interface, it will be moved inside
the container "as is" and renamed to *name*. No additional configuration will
be applied to it.

Example: `--netif=vxlan0,eth0`

macvlan
~~~~~~~

`--netif=macvlan,<master>,<name>`

If the *macvlan* option is used, a new network interface of type `macvlan`
will be created using *master* as master interface, moved inside the container
and renamed to *name*. No additional configuration will be applied to it.

Example: `--netif=macvlan,eth0,eth0`

ipvlan
~~~~~~~

`--netif=ipvlan,<master>,<name>`

If the *ipvlan* option is used, a new network interface of type `ipvlan`
will be created using *master* as master interface, moved inside the container
and renamed to *name*. No additional configuration will be applied to it.

Example: `--netif=ipvlan,eth0,eth0`

veth
~~~~

`--netif=veth,<name_outside>,<name_inside>`

If the *veth* option is used, a new pair of network interfaces of type `veth`
will be created and one of the two moved inside the container. The twin outside
the container will be named *name_outside*, while the twin inside the
container will be named *name_inside*. No additional configuration will be
applied to them.

Example: `--netif=veth,veth0,eth0`

AUTHOR
------

Alessandro Ghedini <alessandro@ghedini.me>

COPYRIGHT
---------

Copyright (C) 2013 Alessandro Ghedini <alessandro@ghedini.me>

This program is released under the 2 clause BSD license.
