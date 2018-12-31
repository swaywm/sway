tray_files = get_option('enable-tray') ? [
	'tray/host.c',
	'tray/icon.c',
	'tray/item.c',
	'tray/tray.c',
	'tray/watcher.c'
] : []

swaybar_deps = [
	cairo,
	client_protos,
	gdk_pixbuf,
	jsonc,
	math,
	pango,
	pangocairo,
	rt,
	wayland_client,
	wayland_cursor,
	wlroots,
]
if get_option('enable-tray')
	if systemd.found()
		swaybar_deps += systemd
	elif elogind.found()
		swaybar_deps += elogind
	endif
endif

executable(
	'swaybar', [
		'bar.c',
		'config.c',
		'i3bar.c',
		'input.c',
		'ipc.c',
		'main.c',
		'render.c',
		'status_line.c',
		tray_files
	],
	include_directories: [sway_inc],
	dependencies: swaybar_deps,
	link_with: [lib_sway_common, lib_sway_client],
	install_rpath : rpathdir,
	install: true
)
