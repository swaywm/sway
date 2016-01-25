include_directories(
	${JSONC_INCLUDE_DIRS}
)

add_executable(swaymsg
	main.c
)

target_link_libraries(swaymsg
	sway-common
	${JSONC_LIBRARIES}
	)

install(
	TARGETS swaymsg
	RUNTIME
	DESTINATION bin
	COMPONENT runtime
)

add_manpage(swaymsg 1)
