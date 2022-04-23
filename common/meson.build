lib_sway_common = static_library(
	'sway-common',
	files(
		'background-image.c',
		'cairo.c',
		'gesture.c',
		'ipc-client.c',
		'log.c',
		'loop.c',
		'list.c',
		'pango.c',
		'stringop.c',
		'util.c'
	),
	dependencies: [
		cairo,
		gdk_pixbuf,
		pango,
		pangocairo,
		wayland_client.partial_dependency(compile_args: true)
	],
	include_directories: sway_inc
)
