project(swaybg)

find_package(Wayland REQUIRED)
find_package(Cairo REQUIRED)
find_package(Pango REQUIRED)

include(Wayland)
set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../bin/)
WAYLAND_ADD_PROTOCOL_CLIENT(proto-xdg-shell
	"${PROJECT_SOURCE_DIR}/../protocols/xdg-shell.xml"
	xdg-shell
)
WAYLAND_ADD_PROTOCOL_CLIENT(proto-desktop-shell
	"${PROJECT_SOURCE_DIR}/../protocols/desktop-shell.xml"
	desktop-shell
)

include_directories(
	${WAYLAND_CLIENT_INCLUDE_DIR}
	${CAIRO_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR}
)

file(GLOB sources ${PROJECT_SOURCE_DIR}/*.c)
file(GLOB common ${PROJECT_SOURCE_DIR}/../common/*.c)
file(GLOB wl_sources ${PROJECT_SOURCE_DIR}/../wayland/*.c)

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
