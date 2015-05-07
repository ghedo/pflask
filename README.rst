pflask
======

.. image:: https://travis-ci.org/ghedo/pflask.png
  :target: https://travis-ci.org/ghedo/pflask

pflask_ is a simple tool for creating Linux namespace containers. It can be
used for running a command or even booting an OS inside an isolated container,
created with the help of Linux namespaces. It is similar in functionality to
`chroot(8)`, although pflask provides better isolation thanks to the use of
namespaces.

Compared to LXC_, pflask is easier to use since it doesn't require any
pre-configuration (all the options can be passed via the command-line). pflask
is mostly intended for testing, building and experimenting, whereas LXC is a
more complete solution, better suited for production environments.

Compared to systemd-nspawn_, pflask doesn't require the use of systemd on the
host system and provides additional options for manipulating mount points and
network interfaces inside the container. On the other hand, systemd-nspawn is
better integrated in the systemd ecosystem.

Additionally, while most other containerization solutions (LXC, systemd-nspawn,
...) are mostly targeted at containing whole systems, pflask can also be used to
contain single programs, without the need to create ad-hoc chroots.

.. _pflask: https://ghedo.github.io/pflask
.. _LXC: http://linuxcontainers.org
.. _systemd-nspawn: http://www.freedesktop.org/software/systemd/man/systemd-nspawn.html

Features
--------

User namespace
~~~~~~~~~~~~~~

When the host system allows it, pflask creates a new user namespace inside the
container, and automatically maps the user running pflask to the root user
inside the container. This means that a user could create and have full root
privileges inside a container, while having none on the host system.

Note that this has been the cause of security vulnerabilities in the past, so
that most OS vendors (reasonably) decided to either disable user namespace
support altogether, or restrict the functionality to root.

pflask can disable the relevant functionality when it detects that support for
user namespaces is not available.

Mount namespace
~~~~~~~~~~~~~~~

By default, pflask creates a new mount namespace inside the container, so that
filesystems mounted inside it won't affect the host system. pflask can also be
told to create new mount points before the execution of the supplied command, 
by using the `--mount` option. Supported mount point types are:

* `bind`    -- bind mount a directory/file to another directory/file
* `bind-ro` -- like `bind`, but read-only
* `overlay` -- stack a directory on top of another directory using AuFS or OVL
* `tmp`     -- mount a tmpfs on a directory

Network namespace
~~~~~~~~~~~~~~~~~

When supplied the `--netif` option, pflask will create a new network namespace
and move/rename the supplied network interface inside the container.

PID, IPC and UTS namespaces
~~~~~~~~~~~~~~~~~~~~~~~~~~~

By default, pflask creates new PID, IPC and UTS namespaces inside the container,
in order to isolate processes, IPC resources and the node/domain name of the
container from the host system.

Examples
--------

Hide directories from an application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   $ pflask --user=$USER --mount=tmp,$HOME chromium --disable-setuid-sandbox

This command does not require a pre-generated chroot (it will use the current
root) and will mount a tmpfs on `$HOME` so that the application (chromium in the
example) won't be able to access your precious files. Any change will be also
discarded once the process terminates. A bind mount can be used to retain the
modifications:

.. code-block:: bash

   $ pflask --user=$USER --mount=bind,/tmp/trash,$HOME  chromium --disable-setuid-sandbox

All filesystem changes applied by the command will be available in /tmp/trash.

Both commands can be run without root privileges as long as user namespaces are
supported by the host system, and available to non-privileged users.

For example, on Debian, user namespaces are enabled, but are restricted to root
only. To enable them for unprivileged users run:

.. code-block:: bash

   $ sudo sysctl kernel.unprivileged_userns_clone=1

Detach from terminal
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   $ pflask --user=$USER --detach /bin/bash

To reattach run pflask with the `--attach` option:

.. code-block:: bash

   $ pidof pflask
   29076
   $ pflask --attach=29076

Where `29076` is the PID of the detached pflask process. Once reattached, one
can detach again by pressing `^@` (Ctrl + @).

Boot the OS inside the container
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First create a base Debian system using `debootstrap(8)`:

.. code-block:: bash

   $ sudo debootstrap --include=systemd unstable /path/to/container

It is recommended to use systemd as init system inside the guest, since it can
detect whether it is run inside a container or not, and disable not needed
services accordingly.

Then create the container:

.. code-block:: bash

   $ sudo pflask --chroot=/path/to/container /lib/systemd/systemd

This will simply execute the init system (systemd) inside the container. Replace
`/lib/systemd/systemd` with `/sbin/init` if you have a different init (but note
that there's no guarantee that it'll work).

Disable network inside the container
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   $ sudo pflask --chroot=/path/to/container --netif /lib/systemd/systemd

Using the `--netif` option without any argument creates a new network namespace
inside the container without adding any new interface, therefore leaving the
_lo_ interface as the only one inside the container and disabling network access
to the outside world while at the same time leaving the network on the host
system working.

Use a private network interface inside the container
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First, let's create the new network interface thet will be used inside the
container:

.. code-block:: bash

   $ sudo ip link add name pflask-vlan0 link eth0 type macvlan

This will create a new interface, `pflask-vlan0`, of type `macvlan` using the
`eth0` interface on the host as master. `macvlan` interfaces can be used to
give a second MAC address to a network adapter (in this case `eth0`) and make
it look like a completely different device.

Finally, create the container:

.. code-block:: bash

   $ sudo pflask --chroot=/path/to/container --netif=pflask-vlan0,eth0 /lib/systemd/systemd

This will take the `pflask-vlan0` interface previously created, move it inside
the container and rename it to `eth0`. The container will thus have what it
looks like a private `eth0` network interface that can be configured
independently from the host `eth0`. Once the container terminates, its network
interface will be destroyed as well.

Note that `macvlan` is just one of the possibilities. One could create a pair
of `veth` interfaces, move one of them inside the container and connect the
other to a bridge (e.g. an Open VSwitch bridge). Alternatively one could create
a `vxlan` interface and connect the container to a VXLAN network, etc...

Copy-on-write filesystem
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   $ sudo pflask --chroot=/path/to/container \
     --mount=overlay,/tmp/overlay/root,/path/to/container,/tmp/overlay/work \
     /lib/systemd/systemd

This will mount a copy-on-write filesystem on the / of the container. Any change
to files and directories will be saved in `/tmp/overlay` so that the container
root directory (`/path/to/container`) will be unaffected.

Note that this requires support for either AuFS or OverlayFS on the host system.

Build a Debian package inside a container
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First, create the base Debian system:

.. code-block:: bash

   $ sudo mkdir -p /var/cache/pflask
   $ DIST=sid pflask-debuild --create

Then retrieve the source package we want to build:

.. code-block:: bash

   $ apt-get source somepackage
   $ cd somepackage-XYX

Where _somepackage_ is the desired package, and _XYZ_ is the package version.

Finally build the package:

.. code-block:: bash

   $ DIST=sid pflask-debuild

The script will take care of creating a new container, installing all the
required dependncies (inside the container), building and signing the package.

A copy-on-write filesystem is also mounted on the / of the container, so that
the same clean chroot can be re-used to build other packages.

Note that the `pflask-debuild`_ tool is far from perfect, and may not work in
all situations.

See the `man page`_ for more information.

.. _`man page`: https://ghedo.github.io/pflask/pflask.html
.. _`pflask-debuild`: https://ghedo.github.io/pflask/pflask-debuild.html

Building
--------

pflask is distributed as source code. Build with:

.. code-block:: bash

   $ ./bootstrap.py
   $ ./waf configure
   $ ./waf build

Copyright
---------

Copyright (C) 2013 Alessandro Ghedini <alessandro@ghedini.me>

See COPYING_ for the license.

.. _COPYING: https://github.com/ghedo/pflask/tree/master/COPYING
