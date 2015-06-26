.. _pflask(1):

pflask
======

SYNOPSIS
--------

.. program:: pflask

**pflask [options] [--] [command ...]**

DESCRIPTION
-----------

**pflask** is a simple tool for creating process containers on LInux. It can be
used for running single commands or even booting a whole operating system
inside an isolated environment, where the filesystem hierarchy, networking,
process tree, IPC subsystems and host/domain name can be insulated from the
host system and other containers.

OPTIONS
-------

.. option:: -r, --chroot=<dir>

   Change the root directory inside the container.

.. option:: -c, --chdir=<dir>

   Change the current directory inside the container.

.. option:: -t, --hostname

   Set the container hostname.

.. option:: -m, --mount=<type>,<opts>

   Create a new *type* mount point inside the container. See MOUNT_ for more
   information.

.. option:: -n, --netif[=<opts>]

   Disconnect the container networking from the host. See NETIF_ for more
   information.

.. option:: -u, --user=<user>

   Run the command under the specified user.

.. option:: -e, --user-map=<map>

   Map container users to host users. The *map* argument is composed of three
   values separated by ``:``: the first userid as seen in the user namespace of
   the container, the first userid as seen on the host, and a range indicating
   the number of consecutive ids to map.

   Example: ``--user-map=0:100000,65536``

.. option:: -w, --volatile

   Discard any change to / once the container exits. This can only be used
   along with ``--chroot`` and requires support for the overlay_ mount type.

.. option:: -g, --cgroup=<controller>

   Create a new cgroup in the given controller and move the container inside
   it.

.. option:: -d, --detach

   Detach from terminal.

.. option:: -a, --attach=<pid>

   Attach to the *pid* detached process. Only a process with the same UID of
   the detached process can attach to it. To detach again press `^@` (Ctrl + @).

.. option:: -s, --setenv=<name>=<value>[,<name>=<value> ...]

   Set additional environment variables. It takes a comma-separated list of
   variables of the form `name=value`. This option may be used more than once.

.. option:: -k, --keepenv

   Do not clear environment (only relevant when used with ``--chroot``).

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

pflask can create the following mount point types using the ``--mount`` option:

bind
~~~~

``--mount=bind,<host_path>,<container_path>``

Bind mounts the *host_path* directory/file on the host filesystem to the
*container_path* directory/file in the container. If the ``--chroot`` option is
used, the destination path will be relative to the chroot directory.

Example: ``--mount=bind,/source/path,/dest/path``

bind-ro
~~~~~~~

``--mount=bind-ro,<host_path>,<container_path>``

Same as ``bind``, but makes the mount point read-only. If the ``--chroot``
option is used, the destination path will be relative to the chroot directory.

Example: ``--mount=bind-ro,/source/path,/dest/path``

overlay
~~~~~~~

``--mount=overla,<root_dir>,<dest>,<work_dir>``

Stacks the host *root_dir* directory on top of the container's *dest* directory
using either AuFS or OverlayFS depending on what is found at compile-time. If
the ``--chroot`` option is used, the destination path will be relative to the
chroot directory. The *work_dir* directory needs to be an empty directory on
the same filesystem as *root_dir*.

Note that AuFS and OverlayFS don't support user namespaces, so the ``--user``
option is incompatible with this mount type unless ``--no-userns`` is also used.

Example: ``--mount=overlay,/overlay/path,/dest/path,/overlay/work``

tmp
~~~

``--mount=tmp,<dest>``

Mounts a temporary in-memory filesystem on the *dest* directory inside the
container.

Example: ``--mount=tmp,/dest/path``

NETIF
-----

pflask will create a new network namespace when the ``--netif`` option is used.
If one of the following arguments is provided, a network interface will also be
created inside the container:

move and rename
~~~~~~~~~~~~~~~

``--netif=<dev>,<name>``

Moves the *dev* network interface from the host to the container, and renames
it to *name*. No additional configuration will be applied to it.

Example: ``--netif=vxlan0,eth0``

macvlan
~~~~~~~

``--netif=macvlan,<master>,<name>``

Creates a ``macvlan`` network interface using *master* as master interface,
moves it inside the container and renames it to *name*. No additional
configuration will be applied to it.

Example: ``--netif=macvlan,eth0,eth0``

ipvlan
~~~~~~~

``--netif=ipvlan,<master>,<name>``

Same as ``macvlan`` but an ``ipvlan`` interface will be created instead. No
additional configuration will be applied to it.

Example: ``--netif=ipvlan,eth0,eth0``

veth
~~~~

``--netif=veth,<name_outside>,<name_inside>``

Creates a pair of ``veth`` network interfaces called *name_outside* and
*name_inside*. The *name_inside* twin will then be moved inside the container.
No additional configuration will be applied to them.

Example: ``--netif=veth,veth0,eth0``

AUTHOR
------

Alessandro Ghedini <alessandro@ghedini.me>

COPYRIGHT
---------

Copyright (C) 2013 Alessandro Ghedini <alessandro@ghedini.me>

This program is released under the 2 clause BSD license.
