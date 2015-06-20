# Copyright (C) 2015 Alessandro Ghedini <alessandro@ghedini.me>
# This file is released under the 2 clause BSD license, see COPYING

import re
from waflib import Utils

APPNAME = 'pflask'
VERSION = '0.2'

_INSTALL_DIRS_LIST = [
	('bindir',  '${DESTDIR}${PREFIX}/bin',      'binary files'),
	('datadir', '${DESTDIR}${PREFIX}/share',    'data files'),
	('docdir',  '${DATADIR}/doc/pflask',        'documentation files'),
	('mandir',  '${DATADIR}/man',               'man pages '),
]

def options(opt):
	opt.load('compiler_c')

	group = opt.get_option_group("build and install options")
	for ident, default, desc in _INSTALL_DIRS_LIST:
		group.add_option('--{0}'.format(ident),
			type    = 'string',
			dest    = ident,
			default = default,
			help    = 'directory for installing {0} [{1}]' \
			            .format(desc, default))

	opt.add_option('--sanitize', action='store', default=None,
	               help='enable specified sanotizer (address, thread, ...)')

def configure(cfg):
	def my_check_cc(ctx, dep, **kw_ext):
		kw_ext['uselib_store'] = dep
		if ctx.check_cc(**kw_ext):
			ctx.env.deps.append(dep)

	def my_check_os(ctx):
		ctx.env.deps.append("os-{0}".format(ctx.env.DEST_OS))

	cfg.load('compiler_c')

	for ident, _, _ in _INSTALL_DIRS_LIST:
		varname = ident.upper()
		cfg.env[varname] = getattr(cfg.options, ident)

		# keep substituting vars, until the paths are fully expanded
		while re.match('\$\{([^}]+)\}', cfg.env[varname]):
			cfg.env[varname] = \
			  Utils.subst_vars(cfg.env[varname], cfg.env)

	cfg.env.CFLAGS   += [ '-Wall', '-Wextra', '-pedantic', '-g', '-std=gnu99' ]
	cfg.env.CPPFLAGS += [ '-D_GNU_SOURCE' ]

	cfg.env.deps = []

	# OS
	my_check_os(cfg)

	# AuFS
	my_check_cc(cfg, 'aufs', header_name='linux/aufs_type.h',
	            define_name='HAVE_AUFS', mandatory=False)

	# sphinx
	cfg.find_program('sphinx-build', mandatory=False)

	if cfg.options.sanitize:
		cflags = [ '-fsanitize=' + cfg.options.sanitize ]
		lflags = [ '-fsanitize=' + cfg.options.sanitize ]

		if cfg.options.sanitize == 'thread':
			cflags += [ '-fPIC' ]
			lflags += [ '-pie' ]

		if cfg.check_cc(cflags=cflags,linkflags=lflags,mandatory=False):
			cfg.env.CFLAGS    += cflags
			cfg.env.LINKFLAGS += lflags

def build(bld):
	sources = [
		# sources
		'src/cgroup.c',
		'src/dev.c',
		'src/mount.c',
		'src/netif.c',
		'src/nl.c',
		'src/path.c',
		'src/pflask.c',
		'src/printf.c',
		'src/pty.c',
		'src/user.c',
		'src/util.c'
	]

	bld.env.append_value('INCLUDES', ['deps', 'src'])

	bld(
		name         = 'pflask',
		features     = 'c cprogram',
		source       = sources,
		target       = 'pflask',
		install_path = bld.env.BINDIR,
	)

	bld.install_files('${BINDIR}', bld.path.ant_glob('tools/pflask-*'),
	                  chmod=Utils.O755)

	if bld.env['SPHINX_BUILD']:
		bld(
			name     = 'docs config',
			features = 'subst',
			source   = 'docs/conf.py.in',
			target   = 'docs/conf.py',
			VERSION  = VERSION,
		)

		bld(
			name   = 'man docs',
			cwd    = 'docs',
			rule   = 'sphinx-build -c ../build/docs/ -b man . ../build/docs/man',
			source = bld.path.ant_glob('docs/pflask.rst') +
			         bld.path.ant_glob('build/docs/conf.py'),
			target = 'docs/man/pflask.1 docs/man/pflask-debuild.1',
			install_path = bld.env.MANDIR
		)

		bld(
			name   = 'html docs',
			cwd    = 'docs',
			rule   = 'sphinx-build -c ../build/docs/ -b html . ../build/docs/html',
			source = bld.path.ant_glob('docs/*.rst') +
			         bld.path.ant_glob('docs/README.rst') +
			         bld.path.ant_glob('build/docs/conf.py'),
			target = 'docs/html/index.html',
		)
