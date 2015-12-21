add_library(sway-common
	ipc-client.c
	list.c
	log.c
	util.c
	readline.c
	stringop.c
	)

if(Backtrace_FOUND)
	set_target_properties(sway-common
		PROPERTIES
		COMPILE_FLAGS "-include ${Backtrace_HEADER}"
		)
endif()
