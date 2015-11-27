project(swaygrab)

set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../bin/)

FILE(GLOB sources ${PROJECT_SOURCE_DIR}/*.c)
FILE(GLOB common ${PROJECT_SOURCE_DIR}/../common/*.c)

add_executable(swaygrab
  ${sources}
  ${common}
)

TARGET_LINK_LIBRARIES(swaygrab rt)

install(
  TARGETS   swaygrab
  RUNTIME   DESTINATION bin
  COMPONENT runtime)
