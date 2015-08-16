cmake_minimum_required(VERSION 2.8.5)
project(sway C)
set(CMAKE_C_FLAGS "-g")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "bin/")
add_definitions("-Wall")
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMake)

find_package(XKBCommon REQUIRED)
find_package(WLC REQUIRED)

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
