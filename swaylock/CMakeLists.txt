include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${WAYLAND_CLIENT_INCLUDE_DIR}
	${CAIRO_INCLUDE_DIRS}
	${GDK_PIXBUF_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${PAM_INCLUDE_DIRS}
)

add_executable(swaylock
	main.c
)

target_link_libraries(swaylock
	sway-common
	sway-wayland
	${WAYLAND_CLIENT_LIBRARIES}
	${WAYLAND_CURSOR_LIBRARIES}
	${CAIRO_LIBRARIES}
	${GDK_PIXBUF_LIBRARIES}
	${PANGO_LIBRARIES}
	${PAM_LIBRARIES}
	m
)

install(
	TARGETS swaylock
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)

install(
	FILES ${CMAKE_CURRENT_SOURCE_DIR}/pam/swaylock
	DESTINATION /etc/pam.d/
	COMPONENT data
)
