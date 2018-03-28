executable(
	'swaybg',
	'main.c',
	include_directories: [sway_inc],
	dependencies: [
		cairo,
		gdk_pixbuf,
		jsonc,
		math,
		pango,
		pangocairo,
		sway_protos,
		wayland_client,
		wlroots,
	],
	link_with: [lib_sway_common, lib_sway_client],
	install: true
)
