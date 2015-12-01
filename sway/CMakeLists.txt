find_package(XKBCommon REQUIRED)
find_package(WLC REQUIRED)
find_package(PCRE REQUIRED)
find_package(Wayland REQUIRED)
find_package(JsonC REQUIRED)

include(Wayland)
WAYLAND_ADD_PROTOCOL_SERVER(proto-desktop-shell
	${PROJECT_SOURCE_DIR}/protocols/desktop-shell.xml
	desktop-shell
)

file(GLOB sources ${PROJECT_SOURCE_DIR}/sway/*.c)
file(GLOB common ${PROJECT_SOURCE_DIR}/common/*.c)

include_directories(
	${WLC_INCLUDE_DIRS}
	${PCRE_INCLUDE_DIRS}
	${JSONC_INCLUDE_DIRS}
	${XKBCOMMON_INCLUDE_DIRS}
	${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(sway
	${sources}
	${common}
	${proto-desktop-shell}
)

target_link_libraries(sway
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
