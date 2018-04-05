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

add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-unused-function', language: 'c')
add_project_arguments('-Wno-unused-result', language: 'c')

cc = meson.get_compiler('c')

is_freebsd = host_machine.system().startswith('freebsd')
datadir = get_option('datadir')
sysconfdir = get_option('sysconfdir')
prefix = get_option('prefix')

jsonc          = dependency('json-c', version: '>=0.13')
pcre           = dependency('libpcre')
wlroots        = dependency('wlroots', fallback: ['wlroots', 'wlroots'])
wayland_server = dependency('wayland-server')
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_egl    = dependency('wayland-egl')
wayland_protos = dependency('wayland-protocols')
xkbcommon      = dependency('xkbcommon')
cairo          = dependency('cairo')
pango          = dependency('pango')
pangocairo     = dependency('pangocairo')
gdk_pixbuf     = dependency('gdk-pixbuf-2.0', required: false)
pixman         = dependency('pixman-1')
libcap         = dependency('libcap')
libinput       = dependency('libinput')
libpam         = cc.find_library('libpam')
math           = cc.find_library('m')
rt             = cc.find_library('rt')
git = find_program('git', required: false)
a2x = find_program('a2x', required: false)

conf_data = configuration_data()

if gdk_pixbuf.found()
	conf_data.set('HAVE_GDK_PIXBUF', true)
endif

if a2x.found()
	mandir = get_option('mandir')
	man_files = [
		'sway/sway.1.txt',
		'sway/sway.5.txt',
		'sway/sway-bar.5.txt',
		'sway/sway-input.5.txt',
		'sway/sway-security.7.txt',
		'swaymsg/swaymsg.1.txt',
	]
	foreach filename : man_files
		topic = filename.split('.')[-3].split('/')[-1]
		section = filename.split('.')[-2]

		custom_target(
			'man-@0@-@1@'.format(topic, section),
			input: filename,
			output: '@BASENAME@',
			command: [
				a2x,
				'--no-xmllint',
				'--doctype', 'manpage',
				'--format', 'manpage',
				'--destination-dir', meson.current_build_dir(),
				'@INPUT@'
			],
			install: true,
			install_dir: '@0@/man@1@'.format(mandir, section)
		)
	endforeach
endif

add_project_arguments('-DSYSCONFDIR="/@0@"'.format(sysconfdir), language : 'c')

version = get_option('sway_version')
if version != ''
	version = '"@0@"'.format(version)
else
	if not git.found()
		error('git is required to make the version string')
	endif

	git_commit_hash = run_command([git.path(), 'describe', '--always', '--tags']).stdout().strip()
	git_branch = run_command([git.path(), 'rev-parse', '--abbrev-ref', 'HEAD']).stdout().strip()
	version = '"@0@ (" __DATE__ ", branch \'@1@\')"'.format(git_commit_hash, git_branch)
endif
add_project_arguments('-DSWAY_VERSION=@0@'.format(version), language: 'c')

sway_inc = include_directories('include')

subdir('include')
subdir('protocols')
subdir('common')
subdir('sway')
subdir('swaymsg')

subdir('client')
subdir('swaybg')
subdir('swaybar')
subdir('swaylock')

config = configuration_data()
config.set('sysconfdir', join_paths(prefix, sysconfdir))
config.set('datadir', join_paths(prefix, datadir))
config.set('prefix', prefix)

configure_file(
	configuration: config,
	input: 'config.in',
	output: '@BASENAME@',
	install_dir: sysconfdir + '/sway'
)

if is_freebsd
	configure_file(
		configuration: config,
		input: 'security.d/10-freebsd.in',
		output: '@BASENAME@',
		install_dir: sysconfdir + '/sway/security.d'
	)
else
	configure_file(
		configuration: config,
		input: 'security.d/00-defaults.in',
		output: '@BASENAME@',
		install_dir: sysconfdir + '/sway/security.d'
	)
endif

install_data(
	'sway.desktop',
	install_dir: datadir + '/wayland-sessions'
)

if (get_option('default_wallpaper'))
	wallpaper_files = files(
		'assets/Sway_Wallpaper_Blue_768x1024.png',
		'assets/Sway_Wallpaper_Blue_768x1024_Portrait.png',
		'assets/Sway_Wallpaper_Blue_1136x640.png',
		'assets/Sway_Wallpaper_Blue_1136x640_Portrait.png',
		'assets/Sway_Wallpaper_Blue_1366x768.png',
		'assets/Sway_Wallpaper_Blue_1920x1080.png',
		'assets/Sway_Wallpaper_Blue_2048x1536.png',
		'assets/Sway_Wallpaper_Blue_2048x1536_Portrait.png',
	)
	wallpaper_install_dir = datadir + '/backgrounds/sway'

	install_data(wallpaper_files, install_dir: wallpaper_install_dir)
endif

if (get_option('zsh_completions'))
	zsh_files = files(
		'completions/zsh/_sway',
		'completions/zsh/_swaygrab',
		'completions/zsh/_swaylock',
		'completions/zsh/_swaymsg',
	)
	zsh_install_dir = datadir + '/zsh/site-functions'

	install_data(zsh_files, install_dir: zsh_install_dir)
endif
