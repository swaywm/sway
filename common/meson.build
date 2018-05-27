lib_sway_common = static_library(
	'sway-common',
	files(
		'background-image.c',
		'cairo.c',
		'log.c',
		'list.c',
		'pango.c',
		'readline.c',
		'stringop.c',
		'unicode.c',
		'util.c'
	),
	dependencies: [
		cairo,
		gdk_pixbuf,
		pango,
		pangocairo,
	],
	include_directories: sway_inc
)
