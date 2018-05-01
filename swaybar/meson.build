executable(
	'swaybar', [
		'bar.c',
		'config.c',
		'event_loop.c',
		'i3bar.c',
		'ipc.c',
		'main.c',
		'render.c',
		'status_line.c',
	],
	include_directories: [sway_inc],
	dependencies: [
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
	],
	link_with: [lib_sway_common, lib_sway_client],
	install: true,
	install_dir: sway_libexecdir
)
