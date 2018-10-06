sysconfdir = get_option('sysconfdir')

dependencies = [
    cairo,
    client_protos,
    gdk_pixbuf,
    math,
    pango,
    pangocairo,
    xkbcommon,
    wayland_client,
    wlroots,
]

sources = [
    'main.c',
    'password.c',
    'render.c',
    'seat.c'
]

if libpam.found()
    sources += ['pam.c']
    dependencies += [libpam]
else
    warning('The swaylock binary must be setuid when compiled without libpam')
    warning('You must do this manually post-install: chmod a+s /path/to/swaylock')
    sources += ['shadow.c']
    if crypt.found()
        dependencies += [crypt]
    endif
endif

executable('swaylock',
    sources,
	include_directories: [sway_inc],
	dependencies: dependencies,
	link_with: [lib_sway_common, lib_sway_client],
	install_rpath : rpathdir,
	install: true
)

if is_freebsd
	install_data(
		'pam/swaylock.freebsd',
		install_dir: sysconfdir + '/pam.d/',
		rename: 'swaylock'
	)
else
	install_data(
		'pam/swaylock.linux',
		install_dir: sysconfdir + '/pam.d/',
		rename: 'swaylock'
	)
endif
