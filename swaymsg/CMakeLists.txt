project(swaymsg)

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
