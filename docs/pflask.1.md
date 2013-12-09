pflask(1) -- the process in the flask
=====================================

## SYNOPSIS

`pflask [OPTIONS] [COMMAND [ARGS...]]`

## DESCRIPTION

**pflask** is a simple tool for creating Linux namespace containers. It can be
used for running a command or even booting an OS inside an isolated container,
created with the help of Linux namespaces. It is similar in functionality to
`chroot(8)`, altough pflask provides better isolation thanks to the use of
namespaces.

## OPTIONS

`-m, --mount <type>,<opts>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Create a new _"type"_ mount point inside the container. See [MOUNT POINTS] for
more info.

`-n, --netif <dev>,<name>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Move the _"dev"_ network interface inside the container and rename it to
_"name"_.

`-u, --user <user>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Run the command as _"user"_.

`-r, --root <dir>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Use _"dir"_ as root directory inside the container.

`-c, --chdir <dir>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Change to _"dir"_  inside the namespace.

`-d, --detach`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Detach from the pflask process, re-attach with --attach.

`-a, --attach <pid>`

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
Attach to the _"pid"_ detached process.

## MOUNT POINTS

pflask support the following mount point types:

### bind

It bind mounts a directory/file to another directory/file

Example: `--mount=bind,/source/path,/dest/path`

### aufs

It stacks a directory on top of another directory using AuFS

Example: `--mount=aufs,/overlay/path,/dest/path`

### loop

It mounts a loop device on a driectory given its file system type.

Example: `--mount=loop,ext4,/path/to/disk.img,/dest/path`

### tmp

It mounts a tmpfs on a directory.

Example: `--mount=tmp,/dest/path`

## AUTHOR ##

Alessandro Ghedini <alessandro@ghedini.me>

## COPYRIGHT ##

Copyright (C) 2013 Alessandro Ghedini <alessandro@ghedini.me>

This program is released under the 2 clause BSD license.
