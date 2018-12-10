lib_sway_common = static_library(
	'sway-common',
	files(
		'background-image.c',
		'cairo.c',
		'ipc-client.c',
		'list.c',
		'log.c',
		'loop.c',
		'pango.c',
		'readline.c',
		'shexp.c',
		'stringop.c',
		'unicode.c',
		'util.c',
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
