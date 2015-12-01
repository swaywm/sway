file(GLOB sources *.c)
file(GLOB common ../common/*.c)

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
