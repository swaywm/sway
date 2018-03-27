deps = [
	cairo,
	jsonc,
	math,
	pango,
	pangocairo,
	sway_protos,
	wayland_client,
]

if gdk_pixbuf.found()
	deps += [gdk_pixbuf]
endif

executable(
    'swaybg',
    'main.c',
    include_directories: [sway_inc],
    dependencies: deps,
    link_with: [lib_sway_common, lib_sway_client],
    install: true
)
