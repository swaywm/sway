include(Wayland)

WAYLAND_ADD_PROTOCOL_CLIENT(proto-client-xdg-shell
	xdg-shell.xml
	xdg-shell
)
WAYLAND_ADD_PROTOCOL_CLIENT(proto-client-desktop-shell
	desktop-shell.xml
	desktop-shell
)
WAYLAND_ADD_PROTOCOL_SERVER(proto-server-desktop-shell
	desktop-shell.xml
	desktop-shell
)

WAYLAND_ADD_PROTOCOL_CLIENT(proto-client-swaylock
	swaylock.xml
	swaylock
)
WAYLAND_ADD_PROTOCOL_SERVER(proto-server-swaylock
	swaylock.xml
	swaylock
)

pkg_check_modules(WLC_NOTIFICATION_AREA "wayland-notification-area >= 3")
pkg_get_variable(WAYLAND_NOTIFICATION_AREA_PKGDATADIR wayland-notification-area pkgdatadir)
WAYLAND_ADD_PROTOCOL_SERVER(proto-server-wayland-notification-area
	${WAYLAND_NOTIFICATION_AREA_PKGDATADIR}/notification-area-unstable-v2.xml
	notification-area-unstable-v2
)

add_library(sway-protocols STATIC
	${proto-client-xdg-shell}
	${proto-client-desktop-shell}
	${proto-server-desktop-shell}
	${proto-client-swaylock}
	${proto-server-swaylock}
	${proto-server-wayland-notification-area}
	)

set(PROTOCOLS_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/protocols PARENT_SCOPE)
INCLUDE_DIRECTORIES(${WAYLAND_INCLUDE_DIR} ${XKBCOMMON_INCLUDE_DIRS})
