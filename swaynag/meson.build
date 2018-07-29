executable(
	'swaynag', [
		'config.c',
		'main.c',
		'render.c',
		'swaynag.c',
		'types.c',
	],
	include_directories: [sway_inc],
	dependencies: [
		cairo,
		client_protos,
		gdk_pixbuf,
		pango,
		pangocairo,
		wayland_client,
		wayland_cursor,
		wlroots,
	],
	link_with: [lib_sway_common, lib_sway_client],
	install: true
)
