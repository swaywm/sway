threads = dependency('threads')

swayidle_deps = [
	client_protos,
	pixman,
	wayland_client,
	wayland_server,
	wlroots,
]

if systemd.found()
	swayidle_deps += systemd
endif
if elogind.found()
	swayidle_deps += elogind
endif

executable(
	'swayidle', [
		'main.c',
	],
	include_directories: [sway_inc],
	dependencies: swayidle_deps,
	link_with: [lib_sway_common, lib_sway_client],
	install_rpath : rpathdir,
	install: true
)
