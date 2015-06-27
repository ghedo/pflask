pflask
======

.. image:: https://travis-ci.org/ghedo/pflask.png
  :target: https://travis-ci.org/ghedo/pflask

pflask_ is a simple tool for creating process containers on LInux. It can be
used for running single commands or even booting a whole operating system
inside an isolated environment, where the filesystem hierarchy, networking,
process tree, IPC subsystems and host/domain name can be insulated from the
host system and other containers.

.. _pflask: https://ghedo.github.io/pflask

Getting Started
---------------

pflask doesn't need any configuration and can be run without any arguments
as follows:

.. code-block:: bash

   $ sudo pflask

By default a new container will be created and a bash shell will be started,
but a custom command can also be specified:

.. code-block:: bash

   $ sudo pflask -- id
   uid=0(root) gid=0(root) gruppi=0(root)

The container can also be run inside a private root directory by using the
``--chroot`` option:

.. code-block:: bash

   $ sudo pflask --chroot=/path/to/rootfs -- id
   uid=0(root) gid=0(root) gruppi=0(root)

This can be used, for example, as a replacement for the ``chroot(8)`` command.
It's even possible to invoke the init binary and boot the whole operating
system inside the container:

.. code-block:: bash

   $ sudo pflask --chroot=/path/to/rootfs -- /sbin/init

Note that pflask doesn't provide any support for creating the rootfs, but can
piggyback on existing tools. For example the ``debootstrap(8)`` command can be
used for creating a Debian rootfs as follows:

.. code-block:: bash

   $ sudo debootstrap sid /path/to/rootfs http://httpredir.debian.org/debian

For more information on pflask usage, have a look at the `man page`_.

.. _`man page`: https://ghedo.github.io/pflask/pflask.html

Networking
~~~~~~~~~~

Using the ``--netif`` option the networking of the container will be
disconnected from the host system and all network interfaces will be made
unavailable to the container:

.. code-block:: bash

   $ sudo pflask --netif -- ip link
   1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default 
       link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00

The ``--netif`` option can also be used to create private network interfaces:

.. code-block:: bash

   $ sudo pflask --netif=macvlan,eth0,net0 -- ip link
   1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default 
       link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
   5: net0@if2: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN mode DEFAULT group default 
       link/ether 92:e4:c2:9b:a4:75 brd ff:ff:ff:ff:ff:ff link-netnsid 0

Interfaces created inside the container will be automatically destroyed once
the container terminates.

The command above will create a new ``macvlan`` interface called ``net0``, from
the ``eth0`` host interface. ``macvlan`` interfaces can be used to give an
additional MAC address to a network adapter and make it look like a completely
different device.

pflask can also create other `types of network interfaces`_, have a look at the
manpage for more information.

.. _`types of network interfaces`: https://ghedo.github.io/pflask/pflask.html#netif

Filesystem
~~~~~~~~~~

By default a new mount namespace is created for the container, so that
filesystems mounted inside it won't affect the host system. The ``--mount``
option can then be used to create new mount points before the execution of the
supplied command.

.. code-block:: bash

   $ sudo pflask --chroot=/path/to/rootfs --mount=bind,/tmp,/tmp

The command above will bind mount the host's ``/tmp`` directory into the
container's ``/tmp``, so that files can be exchanged between them.

pflask can also create other `types of mount points`_, have a look at the
manpage for more information.

.. _`types of mount points`: https://ghedo.github.io/pflask/pflask.html#mount

Volatile root filesystem
~~~~~~~~~~~~~~~~~~~~~~~~

Using the ``--volatile`` option it's possible to tell pflask to discard any
change applied to the root filesystem once the container terminates:

.. code-block:: bash

   $ sudo pflask --chroot=/path/to/rootfs --volatile -- /sbin/init

This can be used for example for a build environment, where dependencies can
be installed at every run on a clean rootfs, without the need to recreate the
rootfs every time.

Unprivileged containers
~~~~~~~~~~~~~~~~~~~~~~~

All the commands above have been executed with root privileges, but pflask can
be invoked, with some limitations, by unprivileged users as well, as long as
user namespaces are supported by the host system.

.. code-block:: bash

   $ pflask --user=$USER -- id
   uid=1000(ghedo) gid=1000(ghedo) gruppi=1000(ghedo)

For example, on recent Debian versions user namespaces are enabled, but are
restricted to the root user only. To enable them for unprivileged users run:

.. code-block:: bash

   $ sudo sysctl kernel.unprivileged_userns_clone=1

This functionality can be used to run every-day user applications such as a
web browser inside a container:

.. code-block:: bash

   $ pflask --user=$USER --mount=tmp,$HOME -- chromium --disable-setuid-sandbox

The command above uses the ``--mount`` option to create a ``tmpfs`` mount point
on the ``$HOME`` directory, so that the application (chromium in the example)
won't be able to access the user's private files, and any modification to the
home directory will be discarded once the container terminates.

The ``--chroot`` option can be used with unprivileged containers as well, but
requires some additional configuration.

The first step is assigning a set of additional UIDs and GIDs to the current
user (``$USER``). These will be used by pflask inside the container:

.. code-block:: bash

   $ sudo usermod --add-subuids 100000-165535 $USER
   $ sudo usermod --add-subgids 100000-165535 $USER

Note that the commands above require root privileges, but have to be run only
once.

Then any time an unprivileged ``chroot(8)`` is needed, the following command
can be run:

.. code-block:: bash

   $ pflask --user-map=0:100000:65536 --chroot=/path/to/rootfs

Note that the ``newuidmap(1)`` and ``newgidmap(1)`` commands need to be
installed for any of this to work: on Debian/Ubuntu systems they are provided
by the ``uidmap`` package.

Background containers
~~~~~~~~~~~~~~~~~~~~~

Containers can be detached from the current terminal as soon as they are
created by using the ``--detach`` option:

.. code-block:: bash

   $ sudo pflask --chroot=/path/to/rootfs --detach

and then later reattached (even to a different terminal) with the ``--attach``
option:

.. code-block:: bash

   $ pidof pflask
   29076
   $ pflask --attach=29076

Where ``29076`` is the PID of the detached pflask process. Once reattached, it
can be detached again by pressing ``^@`` (Ctrl + @).

machined integration
~~~~~~~~~~~~~~~~~~~~

Containers created with pflask are automatically registered with the machined_
daemon, if installed and running. The ``machinectl(1)`` command can then be
used to list and manipulate running containers.

Let's create one container as follows:

.. code-block:: bash

   $ sudo pflask --chroot=/path/to/rootfs -- /sbin/init

Running containers can be listed using the ``list`` command:

.. code-block:: bash

   $ machinectl --no-pager list
   MACHINE      CLASS     SERVICE
   pflask-19170 container pflask

   1 machines listed.

and information regarding a single container can be retrieved with the ``show`` 
command:

.. code-block:: bash

   $ machinectl --no-pager show pflask-19170
   Name=pflask-19170
   Id=00000000000000000000000000000000
   Timestamp=gio 2015-06-25 20:28:34 CEST
   TimestampMonotonic=8860409172
   Service=pflask
   Unit=machine-pflask\x5cx2d19170.scope
   Leader=19170
   Class=container
   RootDirectory=/home/ghedo/local/debian
   State=running

Additionally, the ``status`` command will show more information regarding the
status of the container:

.. code-block:: bash

   $ machinectl --no-pager status pflask-19170
   pflask-19170
   	   Since: gio 2015-06-25 20:28:34 CEST; 1min 21s ago
   	  Leader: 19170 (systemd)
   	 Service: pflask; class container
   	    Root: /home/ghedo/local/debian
   	      OS: Debian GNU/Linux stretch/sid
   	    Unit: machine-pflask\x2d19170.scope
   		  ├─19170 /lib/systemd/systemd
   		  └─system.slice
   		    ├─systemd-journald.service
   		    │ └─19184 /lib/systemd/systemd-journald
   		    └─console-getty.service
   		      └─19216 /sbin/agetty --noclear --keep-baud console 115200 3...
   
   giu 25 20:28:34 kronk systemd[1]: Started Container pflask-19170.
   giu 25 20:28:34 kronk systemd[1]: Starting Container pflask-19170.

One can even log into the container using the ``login`` command (note that
the dbus daemon needs to be running inside the container for this to work):

.. code-block:: bash

   $ sudo machinectl login pflask-19170
   Connected to machine pflask-19170. Press ^] three times within 1s to exit session.

   Debian GNU/Linux stretch/sid kronk pts/0

   kronk login: 

And finally the container can be terminated using either the ``poweroff`` or
``terminate`` commands:

.. code-block:: bash

   $ sudo machinectl poweroff pflask-19170

.. _machined: http://www.freedesktop.org/wiki/Software/systemd/machined/

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
