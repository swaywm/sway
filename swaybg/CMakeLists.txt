project (swaybg)

find_package(Wayland REQUIRED)
find_package(Cairo REQUIRED)
find_package(Pango REQUIRED)

include(Wayland)
WAYLAND_ADD_PROTOCOL_CLIENT(proto-xdg-shell "xdg-shell.xml" xdg-shell)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../bin/")
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${WAYLAND_CLIENT_INCLUDE_DIR}
  ${CAIRO_INCLUDE_DIRS}
  ${PANGO_INCLUDE_DIRS}
)

add_executable(swaybg
	main.c
)

TARGET_LINK_LIBRARIES(swaybg ${WAYLAND_CLIENT_LIBRARIES} ${CAIRO_LIBRARIES} ${PANGO_LIBRARIES})

INSTALL(
  TARGETS swaybg
  RUNTIME DESTINATION bin
)
