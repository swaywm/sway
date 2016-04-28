include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${WAYLAND_INCLUDE_DIR}
	${CAIRO_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${XKBCOMMON_INCLUDE_DIRS}
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
	${PANGO_LIBRARIES}
	m
)

if (WITH_GDK_PIXBUF)
	include_directories(
		${GDK_PIXBUF_INCLUDE_DIRS}
	)
	target_link_libraries(swaybg
		${GDK_PIXBUF_LIBRARIES}
	)
endif()

install(
	TARGETS swaybg
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
