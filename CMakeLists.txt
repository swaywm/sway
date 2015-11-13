cmake_minimum_required(VERSION 3.1.0)

project(sway C)

set(CMAKE_C_FLAGS "-g")
set(CMAKE_C_STANDARD 99)
SET(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
add_definitions("-Wall -Wextra -Wno-unused-parameter -D_GNU_SOURCE")
list(INSERT CMAKE_MODULE_PATH 0
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

add_subdirectory(swaybg swaybg)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND git describe --always
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND  git rev-parse --abbrev-ref HEAD
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")

SET(VERSION_GIT_COMMIT_FLAG "-DSWAY_GIT_VERSION=\"g${GIT_COMMIT_HASH}\"")
add_definitions("${VERSION_GIT_COMMIT_FLAG}")

SET(VERSION_GIT_BRANCH_FLAG "-DSWAY_GIT_BRANCH=\"${GIT_BRANCH}\"")
add_definitions("${VERSION_GIT_BRANCH_FLAG}")

string(TIMESTAMP CURRENT_DATE "%Y-%m-%d" UTC)
add_definitions("-DSWAY_VERSION_DATE=${CURRENT_DATE}")

find_package(XKBCommon REQUIRED)
find_package(WLC REQUIRED)
find_package(A2X REQUIRED)
find_package(PCRE REQUIRED)
find_package(JsonC REQUIRED)

FILE(GLOB sources ${PROJECT_SOURCE_DIR}/sway/*.c)
FILE(GLOB common ${PROJECT_SOURCE_DIR}/common/*.c)

include_directories(
   ${WLC_INCLUDE_DIRS}
   ${PCRE_INCLUDE_DIRS}
   ${JSONC_INCLUDE_DIRS}
   ${XKBCOMMON_INCLUDE_DIRS}
   include/
)

add_executable(sway
    ${sources}
    ${common}
)

target_link_libraries(sway
   ${WLC_LIBRARIES}
   ${XKBCOMMON_LIBRARIES}
   ${PCRE_LIBRARIES}
   ${JSONC_LIBRARIES}
)

INSTALL(
    TARGETS sway
    RUNTIME DESTINATION bin
)

INSTALL(
    FILES ${PROJECT_SOURCE_DIR}/config
    DESTINATION /etc/sway/
)

add_custom_target(man ALL)

function (add_manpage name section)
  add_custom_command(
    OUTPUT  "${CMAKE_CURRENT_BINARY_DIR}/${name}.${section}"
    COMMAND "${A2X_COMMAND}"
            --no-xmllint
            --doctype manpage
            --format manpage
            -D "${CMAKE_CURRENT_BINARY_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${name}.${section}.txt"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${name}.${section}.txt"
    COMMENT "Generating manpage for ${name}.${section}")

  add_custom_target("man-${name}.${section}"
    DEPENDS
      "${CMAKE_CURRENT_BINARY_DIR}/${name}.${section}")
  add_dependencies(man
    "man-${name}.${section}")

  install(
    FILES       "${CMAKE_CURRENT_BINARY_DIR}/${name}.${section}"
    DESTINATION "share/man/man${section}"
    COMPONENT   documentation)
endfunction ()

add_manpage(sway 1)
add_manpage(sway 5)

INSTALL(
  FILES ${PROJECT_SOURCE_DIR}/sway.desktop
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/wayland-sessions/)

INSTALL(
  FILES ${PROJECT_SOURCE_DIR}/sway-xorg.desktop
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/xsessions/)
