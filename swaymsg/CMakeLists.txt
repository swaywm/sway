project(swaymsg)

set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../bin/)

include_directories(
  ${WLC_INCLUDE_DIRS}
)

FILE(GLOB sources ${PROJECT_SOURCE_DIR}/*.c)
FILE(GLOB common ${PROJECT_SOURCE_DIR}/../common/*.c)

add_executable(swaymsg
  ${sources}
  ${common}
)

install(
  TARGETS   swaymsg
  RUNTIME   DESTINATION bin
  COMPONENT runtime)
