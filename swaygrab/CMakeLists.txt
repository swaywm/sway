include_directories(
	${JSONC_INCLUDE_DIRS}
)

add_executable(swaygrab
	main.c
)

target_link_libraries(swaygrab
	sway-common
	${JSONC_LIBRARIES}
	rt
)

install(
	TARGETS swaygrab
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)

add_manpage(swaygrab 1)
