include_directories(
	${WLC_INCLUDE_DIRS}
	${XKBCOMMON_INCLUDE_DIRS}
)

add_library(sway-common STATIC
	ipc-client.c
	list.c
	log.c
	util.c
	readline.c
	stringop.c
	)

target_link_libraries(sway-common m)

if(Backtrace_FOUND)
	set_target_properties(sway-common
		PROPERTIES
		COMPILE_FLAGS "-include ${Backtrace_HEADER}"
		)
endif()
