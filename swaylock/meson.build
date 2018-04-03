executable(
	'swaylock', [
		'main.c',
		'seat.c'
	],
	include_directories: [sway_inc],
	dependencies: [
		cairo,
		client_protos,
		gdk_pixbuf,
		libpam,
		math,
		pango,
		pangocairo,
		xkbcommon,
		wayland_client,
		wlroots,
	],
	link_with: [lib_sway_common, lib_sway_client],
	install: true
)
