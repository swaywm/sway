cmake_minimum_required(VERSION 3.1.0)

project(sway C)

set(FALLBACK_CONFIG_DIR "/etc/sway" CACHE PATH
  "Fallback config directory defaults to /etc/sway")
add_definitions(-DFALLBACK_CONFIG_DIR=\"${FALLBACK_CONFIG_DIR}\")

set(CMAKE_C_FLAGS "-g")
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
add_definitions(
    -D_GNU_SOURCE
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")

list(INSERT CMAKE_MODULE_PATH 0
	${CMAKE_CURRENT_SOURCE_DIR}/CMake
)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
	execute_process(
		COMMAND git describe --always
		OUTPUT_VARIABLE GIT_COMMIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
	execute_process(
		COMMAND git rev-parse --abbrev-ref HEAD
		OUTPUT_VARIABLE GIT_BRANCH
		OUTPUT_STRIP_TRAILING_WHITESPACE
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	)
endif()

add_definitions(-DSWAY_GIT_VERSION=\"${GIT_COMMIT_HASH}\")
add_definitions(-DSWAY_GIT_BRANCH=\"${GIT_BRANCH}\")

string(TIMESTAMP CURRENT_DATE "%Y-%m-%d" UTC)
add_definitions(-DSWAY_VERSION_DATE=\"${CURRENT_DATE}\")

find_package(Cairo REQUIRED)
find_package(JsonC REQUIRED)
find_package(PCRE REQUIRED)
find_package(Pango REQUIRED)
find_package(WLC REQUIRED)
find_package(Wayland REQUIRED)
find_package(XKBCommon REQUIRED)
find_package(GdkPixbuf REQUIRED)
find_package(PAM REQUIRED)

include(Manpage)

include_directories(include)

add_subdirectory(protocols)
add_subdirectory(common)
add_subdirectory(wayland)

add_subdirectory(sway)
add_subdirectory(swaybg)
add_subdirectory(swaymsg)
add_subdirectory(swaygrab)
add_subdirectory(swaybar)
add_subdirectory(swaylock)

install(
	FILES ${CMAKE_CURRENT_SOURCE_DIR}/sway.desktop
	DESTINATION share/wayland-sessions
	COMPONENT data
)
install(
	FILES ${CMAKE_CURRENT_SOURCE_DIR}/sway-xorg.desktop
	DESTINATION share/xsessions
	COMPONENT data
)
