.. _pflask-debuild(1):

pflask-debuild
==============

SYNOPSIS
--------

.. program:: pflask-debuild

**pflask-debuild**

DESCRIPTION
-----------

**pflask-debuild** is a wrapper aroung pflask that builds Debian packages
inside a Linux namespace container. It is also able to run ``lintian(1)`` on
the generated .changes file and sign the resulting package using ``debsign(1)``.

ENVIRONMENT
-----------

DIST
~~~~

The Debian release (e.g. *unstable*). It will be used when searching for the
base Debian install under ``/var/cache/pflask``. By default will be read from
the ``debian/changelog`` of the package to be built.

ARCH
~~~~

The Debian architecture (e.g. *amd64*). It will be used when searching for
the base Debian install under ``/var/cache/pflask``. If none is specified,
``dpkg-architecture(1)`` is used.

DEBUILD_DPKG_BUILDPACKAGE_OPTS
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Options that should be passed to ``dpkg-buildpackage(1)``.

DEBUILD_LINTIAN
~~~~~~~~~~~~~~~

If set to *yes* (default) ``lintian(1)`` will be run.

DEBUILD_LINTIAN_OPTS
~~~~~~~~~~~~~~~~~~~~

Options that should be passed to ``lintian(1)`` (by default *-IE --pedantic*
will be used).

FILES
-----

~/.devscripts
~~~~~~~~~~~~~

User-specific configuration file.

/etc/devscripts.conf
~~~~~~~~~~~~~~~~~~~~

System-wide configuration file.

AUTHOR
------

Alessandro Ghedini <alessandro@ghedini.me>

COPYRIGHT
---------

Copyright (C) 2013 Alessandro Ghedini <alessandro@ghedini.me>

This program is released under the 2 clause BSD license.
