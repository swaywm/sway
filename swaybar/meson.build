executable(
	'swaybar',
	[
		'bar.c',
		'config.c',
		'event_loop.c',
		'ipc.c',
		'main.c',
		'render.c',
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
		wlroots,
	],
	link_with: [lib_sway_common, lib_sway_client],
	install: true
)
