lib_sway_client = static_library(
	'sway-client',
	files(
		'pool-buffer.c',
	),
	dependencies: [
		cairo,
		gdk_pixbuf,
		pango,
		pangocairo,
		wlroots,
		wayland_client,
	],
	link_with: [lib_sway_common],
	include_directories: sway_inc
)
