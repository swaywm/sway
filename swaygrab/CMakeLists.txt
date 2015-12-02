add_executable(swaygrab
	main.c
)

target_link_libraries(swaygrab
	sway-common
	rt
)

install(
	TARGETS swaygrab
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
