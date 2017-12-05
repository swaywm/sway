sway_common_deps = [
	wlroots
]

lib_sway_common = static_library('sway-common',
	files(
		'log.c',
		'list.c',
		'util.c',
		'stringop.c',
		'readline.c',
		'ipc-client.c'
	),
	include_directories: sway_inc,
	dependencies: [sway_common_deps]
)
