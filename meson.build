project(
	'sway',
	'c',
	license: 'MIT',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

cc = meson.get_compiler('c')

jsonc          = dependency('json-c', version: '>=0.12.1')
pcre           = dependency('libpcre')
wlroots        = dependency('wlroots')
wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
wayland_egl    = dependency('wayland-egl')
wayland_protos = dependency('wayland-protocols')
xkbcommon      = dependency('xkbcommon')
pango          = dependency('pango')
pixman         = dependency('pixman-1')
libcap         = dependency('libcap')
math           = cc.find_library('m')

# TODO version
add_project_arguments('-DSWAY_VERSION="5"', language: 'c')

sway_inc = include_directories('include')

subdir('common')
subdir('sway')

