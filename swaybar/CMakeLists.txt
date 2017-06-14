include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${WAYLAND_INCLUDE_DIR}
	${CAIRO_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${JSONC_INCLUDE_DIRS}
	${XKBCOMMON_INCLUDE_DIRS}
	${DBUS_INCLUDE_DIRS}
)
if (enable-tray)
	file(GLOB tray
		tray/*.c
	)
endif()

add_executable(swaybar
	main.c
	config.c
	render.c
	bar.c
	status_line.c
	ipc.c
	event_loop.c
	${tray}
)

target_link_libraries(swaybar
	sway-common
	sway-wayland
	${WAYLAND_CLIENT_LIBRARIES}
	${WAYLAND_CURSOR_LIBRARIES}
	${CAIRO_LIBRARIES}
	${PANGO_LIBRARIES}
	${JSONC_LIBRARIES}
	${DBUS_LIBRARIES}
)

if (WITH_GDK_PIXBUF)
	include_directories(
		${GDK_PIXBUF_INCLUDE_DIRS}
	)
endif()

target_link_libraries(swaybar rt)

install(
	TARGETS swaybar
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
