cmake_minimum_required(VERSION 3.1.0)

project(sway C)

set(CMAKE_C_FLAGS "-g")
set(CMAKE_C_STANDARD 99)
SET(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CURRENT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_definitions("-Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE")
list(INSERT CMAKE_MODULE_PATH 0
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND git describe --always
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    execute_process(
        COMMAND  git rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")

add_definitions("-DSWAY_GIT_VERSION=\"g${GIT_COMMIT_HASH}\"")
add_definitions("-DSWAY_GIT_BRANCH=\"${GIT_BRANCH}\"")

string(TIMESTAMP CURRENT_DATE "%Y-%m-%d" UTC)
add_definitions('-DSWAY_VERSION_DATE="${CURRENT_DATE}"')

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

add_subdirectory(swaybg)
add_subdirectory(swaymsg)

find_package(XKBCommon REQUIRED)
find_package(WLC REQUIRED)
find_package(A2X REQUIRED)
find_package(PCRE REQUIRED)
find_package(Wayland REQUIRED)
find_package(JsonC REQUIRED)

FILE(GLOB sources ${PROJECT_SOURCE_DIR}/sway/*.c)
FILE(GLOB common ${PROJECT_SOURCE_DIR}/common/*.c)

include(Wayland)
WAYLAND_ADD_PROTOCOL_SERVER(proto-desktop-shell "${PROJECT_SOURCE_DIR}/protocols/desktop-shell.xml" desktop-shell)

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
  TARGETS   sway
  RUNTIME   DESTINATION bin
  COMPONENT runtime)
install(
  FILES       "${CMAKE_CURRENT_SOURCE_DIR}/config"
  DESTINATION /etc/sway/
  COMPONENT   configuration)

add_custom_target(man ALL)

function (add_manpage name section)
  add_custom_command(
    OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/${name}.${section}"
    COMMAND "${A2X_COMMAND}"
            --no-xmllint
            --doctype manpage
            --format manpage
            -D "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${name}.${section}.txt"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${name}.${section}.txt"
    COMMENT "Generating manpage for ${name}.${section}")

  add_custom_target("man-${name}.${section}"
    DEPENDS
      "${CMAKE_CURRENT_BINARY_DIR}/${name}.${section}")
  add_dependencies(man
    "man-${name}.${section}")

  install(
    FILES       "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${name}.${section}"
    DESTINATION "share/man/man${section}"
    COMPONENT   documentation)
endfunction ()

add_manpage(sway 1)
add_manpage(sway 5)

install(
  FILES       "${CMAKE_CURRENT_SOURCE_DIR}/sway.desktop"
  DESTINATION share/wayland-sessions
  COMPONENT   data)
install(
  FILES       "${CMAKE_CURRENT_SOURCE_DIR}/sway-xorg.desktop"
  DESTINATION share/xsessions
  COMPONENT   data)
