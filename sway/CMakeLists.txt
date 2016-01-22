include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${WLC_INCLUDE_DIRS}
	${PCRE_INCLUDE_DIRS}
	${JSONC_INCLUDE_DIRS}
	${XKBCOMMON_INCLUDE_DIRS}
	${LIBINPUT_INCLUDE_DIRS}
)

add_executable(sway
	commands.c
	config.c
	container.c
	criteria.c
	debug_log.c
	extensions.c
	focus.c
	handlers.c
	input.c
	input_state.c
	ipc-server.c
	layout.c
	main.c
	output.c
	resize.c
	workspace.c
)

add_definitions(
	-DSYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}"
)

target_link_libraries(sway
	sway-common
	sway-protocols
	${WLC_LIBRARIES}
	${XKBCOMMON_LIBRARIES}
	${PCRE_LIBRARIES}
	${JSONC_LIBRARIES}
	${WAYLAND_SERVER_LIBRARIES}
	${LIBINPUT_LIBRARIES}
	m
)

install(
	TARGETS sway
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
install(
	FILES ${PROJECT_SOURCE_DIR}/config
	DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/sway/
	COMPONENT configuration
)

add_manpage(sway 1)
add_manpage(sway 5)
