executable(
    'swaybg',
    'main.c',
    include_directories: [sway_inc],
    dependencies: [wayland_client, sway_protos, jsonc, wlroots],
    link_with: [lib_sway_common],
    install: true
)
