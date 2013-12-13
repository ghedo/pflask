pflask(1) -- the process in the flask
=====================================

## SYNOPSIS

`pflask [OPTIONS] [COMMAND [ARGS...]]`

## DESCRIPTION

**pflask** is a simple tool for creating Linux namespace containers. It can be
used for running a command or even booting an OS inside an isolated container,
created with the help of Linux namespaces. It is similar in functionality to
`chroot(8)`, although pflask provides better isolation thanks to the use of
namespaces. Unlike chroot and most other containerization solutions, pflask can
also be used without changing the root directory inside the container.

## OPTIONS

`-m, --mount=<type>,<opts>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Create a new _"type"_ mount point inside the container. See [MOUNT POINTS] for
more info.

`-n, --netif[=<dev>,<name>]`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Create a new network namespace and if the argument is present, move the _"dev"_
network interface inside the container and rename it to _"name"_. Once the
container terminates, this interface will be destroyed.

`-u, --user=<user>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Run the command as _"user"_ inside the container.

`-r, --root=<dir>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Use _"dir"_ as root directory inside the container.

`-c, --chdir=<dir>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Change to _"dir"_  inside the container.

`-d, --detach`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Detach from terminal.

`-a, --attach=<pid>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Attach to the _"pid"_ detached process. Only a process with the same UID of the
detached process can attach to it. To detach again press `^@` (Ctrl + @).

`-U, --no-userns`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Disable user namespace.

`-M, --no-mountns`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Disable mount namespace.

`-N, --no-netns`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Disable net namespace.

`-I, --no-ipcns`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Disable IPC namespace.

`-H, --no-utsns`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Disable UTS namespace.

`-P, --no-pidrns`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Disable PID namespace.

## MOUNT POINTS

pflask support the following mount point types:

### bind

It bind mounts a directory/file to another directory/file

Example: `--mount=bind,/source/path,/dest/path`

### aufs

It stacks a directory on top of another directory using AuFS

Example: `--mount=aufs,/overlay/path,/dest/path`

### tmp

It mounts a tmpfs on a directory.

Example: `--mount=tmp,/dest/path`

## AUTHOR ##

Alessandro Ghedini <alessandro@ghedini.me>

## COPYRIGHT ##

Copyright (C) 2013 Alessandro Ghedini <alessandro@ghedini.me>

This program is released under the 2 clause BSD license.
