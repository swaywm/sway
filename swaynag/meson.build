executable(
	'swaynag', [
		'config.c',
		'main.c',
		'render.c',
		'swaynag.c',
		'types.c',
		wl_protos_src,
	],
	include_directories: [sway_inc],
	dependencies: [
		cairo,
		pango,
		pangocairo,
		rt,
		wayland_client,
		wayland_cursor,
	],
	link_with: [lib_sway_common, lib_sway_client],
	install: true
)
