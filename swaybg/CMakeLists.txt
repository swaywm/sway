include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${WAYLAND_CLIENT_INCLUDE_DIR}
	${CAIRO_INCLUDE_DIRS}
	${GDK_PIXBUF_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
)

add_executable(swaybg
	main.c
)

target_link_libraries(swaybg
	sway-common
	sway-wayland
	${WAYLAND_CLIENT_LIBRARIES}
	${WAYLAND_CURSOR_LIBRARIES}
	${CAIRO_LIBRARIES}
	${GDK_PIXBUF_LIBRARIES}
	${PANGO_LIBRARIES}
	m
)

install(
	TARGETS swaybg
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
