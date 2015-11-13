project (swaybg)

find_package(Wayland REQUIRED)
find_package(Cairo REQUIRED)
find_package(Pango REQUIRED)

include(Wayland)
WAYLAND_ADD_PROTOCOL_CLIENT(proto-xdg-shell "xdg-shell.xml" xdg-shell)

include_directories(
  ${WAYLAND_CLIENT_INCLUDE_DIR}
  ${CAIRO_INCLUDE_DIRS}
  ${PANGO_INCLUDE_DIRS}
)

FILE(GLOB sources ${PROJECT_SOURCE_DIR}/*.c)
FILE(GLOB common ${PROJECT_SOURCE_DIR}/../common/*.c)
FILE(GLOB wl_sources ${PROJECT_SOURCE_DIR}/../wayland/*.c)

add_executable(swaybg
  ${sources}
  ${wl_sources}
  ${common}
)

TARGET_LINK_LIBRARIES(swaybg ${WAYLAND_CLIENT_LIBRARIES} ${CAIRO_LIBRARIES} ${PANGO_LIBRARIES})

install(
  TARGETS   swaybg
  RUNTIME   DESTINATION bin
  COMPONENT runtime)
