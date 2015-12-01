find_package(Wayland REQUIRED)
find_package(Cairo REQUIRED)
find_package(Pango REQUIRED)

include(Wayland)
WAYLAND_ADD_PROTOCOL_CLIENT(proto-xdg-shell
	../protocols/xdg-shell.xml
	xdg-shell
)
WAYLAND_ADD_PROTOCOL_CLIENT(proto-desktop-shell
	../protocols/desktop-shell.xml
	desktop-shell
)

include_directories(
	${WAYLAND_CLIENT_INCLUDE_DIR}
	${CAIRO_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR}
)

file(GLOB sources *.c)
file(GLOB common ../common/*.c)
file(GLOB wl_sources ../wayland/*.c)

add_executable(swaybg
	${sources}
	${wl_sources}
	${common}
	${proto-xdg-shell}
	${proto-desktop-shell}
)

target_link_libraries(swaybg
	${WAYLAND_CLIENT_LIBRARIES}
	${WAYLAND_CURSOR_LIBRARIES}
	${CAIRO_LIBRARIES}
	${PANGO_LIBRARIES}
	m
)

install(
	TARGETS swaybg
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
