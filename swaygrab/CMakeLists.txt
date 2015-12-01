file(GLOB sources *.c)
file(GLOB common ../common/*.c)

add_executable(swaygrab
	${sources}
	${common}
)

target_link_libraries(swaygrab
	rt
)

install(
	TARGETS swaygrab
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
