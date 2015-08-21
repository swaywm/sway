cmake_minimum_required(VERSION 2.8.5)
project(sway C)
set(CMAKE_C_FLAGS "-g")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "bin/")
add_definitions("-Wall -Wextra -Wno-unused-parameter")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMake)

find_package(XKBCommon REQUIRED)
find_package(WLC REQUIRED)
find_package(A2X REQUIRED)

if (UNIX)
   find_library(DL_LIBRARY dl)
   mark_as_advanced(DL_LIBRARY)
   if (NOT DL_LIBRARY)
      message(FATAL_ERROR "libdl is needed on unix systems")
   endif ()
endif (UNIX)

FILE(GLOB sources ${PROJECT_SOURCE_DIR}/sway/*.c)

include_directories(
   ${WLC_INCLUDE_DIRS}
   include/
)

add_executable(sway
    ${sources}
)

target_link_libraries(sway
   ${WLC_LIBRARIES}
   ${XKBCOMMON_LIBRARIES}
   ${DL_LIBRARY}
)

INSTALL(
    TARGETS sway
    RUNTIME DESTINATION bin
)

INSTALL(
    FILES ${PROJECT_SOURCE_DIR}/config
    DESTINATION /etc/sway/
)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sway.1
  COMMAND ${A2X_COMMAND} --no-xmllint --doctype manpage --format manpage
    -D ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_CURRENT_SOURCE_DIR}/sway.1.txt 
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sway.1.txt
)

ADD_CUSTOM_COMMAND(OUTPUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sway.5
  COMMAND ${A2X_COMMAND} --no-xmllint --doctype manpage --format manpage
    -D ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} ${CMAKE_CURRENT_SOURCE_DIR}/sway.5.txt 
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/sway.5.txt
)

ADD_CUSTOM_TARGET(man ALL
  DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sway.1
  DEPENDS ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sway.5)

INSTALL(
  FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sway.1
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1
)

INSTALL(
  FILES ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/sway.5
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man5
)

INSTALL(
  FILES ${PROJECT_SOURCE_DIR}/sway.desktop
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/wayland-sessions/)

INSTALL(
  FILES ${PROJECT_SOURCE_DIR}/sway-xorg.desktop
  DESTINATION ${CMAKE_INSTALL_PREFIX}/share/xsessions/)
