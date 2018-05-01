executable(
	'swaybg',
	'main.c',
	include_directories: [sway_inc],
	dependencies: [
		cairo,
		client_protos,
		gdk_pixbuf,
		jsonc,
		math,
		pango,
		pangocairo,
		wayland_client,
		wlroots,
	],
	link_with: [lib_sway_common, lib_sway_client],
	install: true,
	install_dir: instlibdir
)
