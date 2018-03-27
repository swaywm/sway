deps = [
	cairo,
	pango,
	pangocairo,
	wlroots,
	wayland_client,
]

if gdk_pixbuf.found()
	deps += [gdk_pixbuf]
endif

lib_sway_client = static_library(
	'sway-client',
	files(
		'buffer-pool.c',
	),
	dependencies: deps,
	link_with: [lib_sway_common],
	include_directories: sway_inc
)
