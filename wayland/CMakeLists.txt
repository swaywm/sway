include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${XKBCOMMON_INCLUDE_DIRS}
	${WAYLAND_INCLUDE_DIR}
)

add_library(sway-wayland
	buffers.c
	pango.c
	registry.c
	window.c
	cairo.c
	)

target_link_libraries(sway-wayland
	sway-common
	sway-protocols
	${PANGO_LIBRARIES}
	${XKBCOMMON_LIBRARIES}
	)

if (WITH_GDK_PIXBUF)
  include_directories(
	  ${GDK_PIXBUF_INCLUDE_DIRS}
  )
  target_link_libraries(sway-wayland
	  ${GDK_PIXBUF_LIBRARIES}
  )
endif()
