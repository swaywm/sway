lib_sway_common = static_library(
	'sway-common',
	files(
		'background-image.c',
		'cairo.c',
		'ipc-client.c',
		'log.c',
		'list.c',
		'pango.c',
		'readline.c',
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
