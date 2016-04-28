include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${WAYLAND_INCLUDE_DIR}
	${CAIRO_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${JSONC_INCLUDE_DIRS}
	${XKBCOMMON_INCLUDE_DIRS}
)

add_executable(swaybar
	main.c
	config.c
	render.c
	bar.c
	status_line.c
	ipc.c
)

target_link_libraries(swaybar
	sway-common
	sway-wayland
	${WAYLAND_CLIENT_LIBRARIES}
	${WAYLAND_CURSOR_LIBRARIES}
	${CAIRO_LIBRARIES}
	${PANGO_LIBRARIES}
	${JSONC_LIBRARIES}
)

if (WITH_GDK_PIXBUF)
	include_directories(
		${GDK_PIXBUF_INCLUDE_DIRS}
	)
endif()

install(
	TARGETS swaybar
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
