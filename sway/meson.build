sway_sources = files(
	'main.c',
	'server.c',
	'commands.c',
	'input/input-manager.c',
	'input/seat.c',
	'commands/exit.c',
	'commands/exec.c',
	'commands/exec_always.c',
	'config.c',
	'ipc-json.c',
	'ipc-server.c',
	'desktop/output.c',
	'desktop/wl_shell.c',
	'desktop/xdg_shell_v6.c',
	'desktop/xwayland.c',
	'security.c',
	'tree/container.c',
	'tree/layout.c',
	'tree/workspace.c',
)

sway_deps = [
	pixman,
	wayland_server,
	jsonc,
	wlroots,
	libcap,
	math,
	libinput,
]

executable(
	'sway',
	sway_sources,
	include_directories: [sway_inc],
	dependencies: sway_deps,
	link_with: [lib_sway_common],
	install: true
)
