lib_sway_common = static_library(
	'sway-common',
	files(
		'background-image.c',
		'cairo.c',
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
		wlroots
	],
	include_directories: sway_inc
)

lib_sway_ipc_client = static_library(
	'sway-ipc-client',
	files(
		'ipc-client.c',
	),
	dependencies: [],
	include_directories: sway_inc
)
