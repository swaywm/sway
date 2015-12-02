include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${WLC_INCLUDE_DIRS}
	${PCRE_INCLUDE_DIRS}
	${JSONC_INCLUDE_DIRS}
	${XKBCOMMON_INCLUDE_DIRS}
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
	input_state.c
	ipc-server.c
	layout.c
	main.c
	output.c
	resize.c
	util.c
	workspace.c
)

target_link_libraries(sway
	sway-common
	sway-protocols
	${WLC_LIBRARIES}
	${XKBCOMMON_LIBRARIES}
	${PCRE_LIBRARIES}
	${JSONC_LIBRARIES}
	${WAYLAND_SERVER_LIBRARIES}
)

install(
	TARGETS sway
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
install(
	FILES ${PROJECT_SOURCE_DIR}/config
	DESTINATION ${FALLBACK_CONFIG_DIR}
	COMPONENT configuration
)
