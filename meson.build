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

git_commit_hash = run_command(['git', 'describe', '--always', '--tags']).stdout().strip()
git_branch = run_command(['git', 'rev-parse', '--abbrev-ref', 'HEAD']).stdout().strip()
version = '"@0@ (" __DATE__ ", branch \'@1@\')"'.format(git_commit_hash, git_branch)

add_project_arguments('-DSWAY_VERSION=@0@'.format(version), language: 'c')

sway_inc = include_directories('include')

subdir('common')
subdir('sway')

