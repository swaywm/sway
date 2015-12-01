project(swaygrab)

set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../bin/)

file(GLOB sources ${PROJECT_SOURCE_DIR}/*.c)
file(GLOB common ${PROJECT_SOURCE_DIR}/../common/*.c)

add_executable(swaygrab
  ${sources}
  ${common}
)

target_link_libraries(swaygrab rt)

install(
  TARGETS   swaygrab
  RUNTIME   DESTINATION bin
  COMPONENT runtime)
