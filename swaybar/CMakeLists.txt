include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${WAYLAND_CLIENT_INCLUDE_DIR}
	${CAIRO_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${JSONC_INCLUDE_DIRS}
)

add_executable(swaybar
	main.c
)

target_link_libraries(swaybar
	sway-common
	sway-wayland
	${WAYLAND_CLIENT_LIBRARIES}
	${WAYLAND_CURSOR_LIBRARIES}
	${CAIRO_LIBRARIES}
	${PANGO_LIBRARIES}
	${JSONC_LIBRARIES}
	m
)

install(
	TARGETS swaybar
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
