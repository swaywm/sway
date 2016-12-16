include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${WLC_INCLUDE_DIRS}
	${PCRE_INCLUDE_DIRS}
	${JSONC_INCLUDE_DIRS}
	${XKBCOMMON_INCLUDE_DIRS}
	${LIBINPUT_INCLUDE_DIRS}
	${CAIRO_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${WAYLAND_INCLUDE_DIR}
)

file(GLOB cmds
	"commands/*.c"
	"commands/bar/*.c"
	"commands/input/*.c"
)

add_executable(sway
	commands.c
	${cmds}
	config.c
	container.c
	criteria.c
	debug_log.c
	extensions.c
	focus.c
	handlers.c
	input.c
	input_state.c
	ipc-json.c
	ipc-server.c
	layout.c
	main.c
	output.c
	workspace.c
	border.c
    security.c
)

add_definitions(
	-DSYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}"
)

target_link_libraries(sway
	sway-common
	sway-protocols
	sway-wayland
	${WLC_LIBRARIES}
	${XKBCOMMON_LIBRARIES}
	${PCRE_LIBRARIES}
	${JSONC_LIBRARIES}
	${WAYLAND_SERVER_LIBRARIES}
	${LIBINPUT_LIBRARIES}
	${PANGO_LIBRARIES}
	${JSONC_LIBRARIES}
	m
)

if (CMAKE_SYSTEM_NAME STREQUAL Linux)
	target_link_libraries(sway cap)
endif (CMAKE_SYSTEM_NAME STREQUAL Linux)

install(
	TARGETS sway
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)

add_custom_target(configs ALL)

function(add_config name source destination)
    add_custom_command(
        OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}
        COMMAND sed -r
            's?__PREFIX__?${CMAKE_INSTALL_PREFIX}?g\; s?__SYSCONFDIR__?${CMAKE_INSTALL_FULL_SYSCONFDIR}?g\; s?__DATADIR__?${CMAKE_INSTALL_FULL_DATADIR}?g'
            ${PROJECT_SOURCE_DIR}/${source}.in > ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}
        DEPENDS ${PROJECT_SOURCE_DIR}/${source}.in
        COMMENT "Generating config file ${source}"
    )

    install(
        FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/${destination}
        COMPONENT configuration
    )

	add_custom_target(config-${name} DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name})
    add_dependencies(configs config-${name})
endfunction()

add_config(config config sway)
add_config(security config.d/security sway/config.d)

add_manpage(sway 1)
add_manpage(sway 5)
add_manpage(sway-input 5)
add_manpage(sway-bar 5)
add_manpage(sway-security 7)
