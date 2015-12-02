add_executable(swaymsg
	main.c
)

target_link_libraries(swaymsg
	sway-common
	)

install(
	TARGETS swaymsg
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)
