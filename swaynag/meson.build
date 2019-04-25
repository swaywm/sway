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
		pango,
		pangocairo,
		wayland_client,
		wayland_cursor,
	],
	link_with: [lib_sway_common, lib_sway_client],
	install: true
)
