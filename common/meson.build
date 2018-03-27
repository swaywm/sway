deps = [
	cairo,
	wlroots
]

if gdk_pixbuf.found()
	deps += [gdk_pixbuf]
endif

lib_sway_common = static_library(
	'sway-common',
	files(
		'cairo.c',
		'ipc-client.c',
		'log.c',
		'list.c',
		'readline.c',
		'stringop.c',
		'util.c'
	),
	dependencies: deps,
	include_directories: sway_inc
)
