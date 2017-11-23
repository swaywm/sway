include_directories(
	${PROTOCOLS_INCLUDE_DIRS}
	${WLR_INCLUDE_DIRS}
	${PCRE_INCLUDE_DIRS}
	${JSONC_INCLUDE_DIRS}
	${XKBCOMMON_INCLUDE_DIRS}
	${LIBINPUT_INCLUDE_DIRS}
	${CAIRO_INCLUDE_DIRS}
	${PANGO_INCLUDE_DIRS}
	${WAYLAND_INCLUDE_DIR}
)

add_executable(sway
    desktop/output.c
    desktop/xdg_shell_v6.c

    tree/container.c
    tree/layout.c
    tree/workspace.c

	base64.c
	main.c
    server.c
)

add_definitions(
	-DSYSCONFDIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}"
)

target_link_libraries(sway
	sway-common
	sway-protocols
	sway-wayland
	${WLR_LIBRARIES}
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
add_config(00-defaults security.d/00-defaults sway/security.d)

if (CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    add_config(10-freebsd security.d/10-freebsd sway/security.d)
endif (CMAKE_SYSTEM_NAME STREQUAL FreeBSD)

if (A2X_FOUND)
    add_manpage(sway 1)
    add_manpage(sway 5)
    add_manpage(sway-input 5)
    add_manpage(sway-bar 5)
    add_manpage(sway-security 7)
endif()
