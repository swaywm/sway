executable(
    'swaymsg',
    'main.c',
    include_directories: [sway_inc],
    dependencies: [jsonc, wlroots],
    link_with: [lib_sway_common, lib_sway_ipc],
    install: true
)
