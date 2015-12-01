project(swaymsg)

set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/../bin/)

file(GLOB sources ${PROJECT_SOURCE_DIR}/*.c)
file(GLOB common ${PROJECT_SOURCE_DIR}/../common/*.c)

add_executable(swaymsg
	${sources}
	${common}
)

install(
	TARGETS swaymsg
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
