project(
	'sway',
	'c',
	version: '1.9-dev',
	license: 'MIT',
	meson_version: '>=0.60.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
		'werror=true',
	],
)

add_project_arguments(
	[
		'-DWLR_USE_UNSTABLE',

		'-Wno-unused-parameter',
		'-Wno-unused-result',
		'-Wno-missing-braces',
		'-Wundef',
		'-Wvla',
	],
	language: 'c',
)

cc = meson.get_compiler('c')

is_freebsd = host_machine.system().startswith('freebsd')
datadir = get_option('datadir')
sysconfdir = get_option('sysconfdir')
prefix = get_option('prefix')

if is_freebsd
	add_project_arguments('-D_C11_SOURCE', language: 'c')
endif

# Execute the wlroots subproject, if any
wlroots_version = ['>=0.17.0', '<0.18.0']
subproject(
	'wlroots',
	default_options: ['examples=false'],
	required: false,
	version: wlroots_version,
)
wlroots = dependency('wlroots', version: wlroots_version)
wlroots_features = {
	'xwayland': false,
	'libinput_backend': false,
	'gles2_renderer': false,
	'session': false,
}
foreach name, _ : wlroots_features
	var_name = 'have_' + name.underscorify()
	have = wlroots.get_variable(pkgconfig: var_name, internal: var_name) == 'true'
	wlroots_features += { name: have }
endforeach

if get_option('xwayland').enabled() and not wlroots_features['xwayland']
	error('Cannot enable Xwayland in sway: wlroots has been built without Xwayland support')
endif

null_dep = dependency('', required: false)

jsonc = dependency('json-c', version: '>=0.13')
pcre2 = dependency('libpcre2-8')
wayland_server = dependency('wayland-server', version: '>=1.21.0')
wayland_client = dependency('wayland-client')
wayland_cursor = dependency('wayland-cursor')
wayland_protos = dependency('wayland-protocols', version: '>=1.24')
xkbcommon = dependency('xkbcommon', version: '>=1.5.0')
cairo = dependency('cairo')
pango = dependency('pango')
pangocairo = dependency('pangocairo')
gdk_pixbuf = dependency('gdk-pixbuf-2.0', required: get_option('gdk-pixbuf'))
pixman = dependency('pixman-1')
glesv2 = wlroots_features['gles2_renderer'] ? dependency('glesv2') : null_dep
libevdev = dependency('libevdev')
libinput = wlroots_features['libinput_backend'] ? dependency('libinput', version: '>=1.21.0') : null_dep
xcb = dependency('xcb', required: get_option('xwayland'))
drm_full = dependency('libdrm') # only needed for drm_fourcc.h
drm = drm_full.partial_dependency(compile_args: true, includes: true)
libudev = wlroots_features['libinput_backend'] ? dependency('libudev') : null_dep
math = cc.find_library('m')
rt = cc.find_library('rt')
xcb_icccm = dependency('xcb-icccm', required: get_option('xwayland'))
threads = dependency('threads') # for pthread_setschedparam

have_xwayland = xcb.found() and xcb_icccm.found() and wlroots_features['xwayland']

if get_option('sd-bus-provider') == 'auto'
	if not get_option('tray').disabled()
		assert(get_option('auto_features').auto(), 'sd-bus-provider must not be set to auto since auto_features != auto')
	endif
	sdbus = dependency(['libsystemd', 'libelogind'],
		required: false,
		version: '>=239',
	)
	if not sdbus.found()
		sdbus = dependency('basu', required: false)
	endif
else
	sdbus = dependency(get_option('sd-bus-provider'), required: get_option('tray'))
endif

tray_deps_found = sdbus.found()
if get_option('tray').enabled() and not tray_deps_found
	error('Building with -Dtray=enabled, but sd-bus has not been not found')
endif
have_tray = (not get_option('tray').disabled()) and tray_deps_found

conf_data = configuration_data()

conf_data.set10('HAVE_XWAYLAND', have_xwayland)
conf_data.set10('HAVE_GDK_PIXBUF', gdk_pixbuf.found())
conf_data.set10('HAVE_LIBSYSTEMD', sdbus.found() and sdbus.name() == 'libsystemd')
conf_data.set10('HAVE_LIBELOGIND', sdbus.found() and sdbus.name() == 'libelogind')
conf_data.set10('HAVE_BASU', sdbus.found() and sdbus.name() == 'basu')
conf_data.set10('HAVE_TRAY', have_tray)

scdoc = dependency('scdoc', version: '>=1.9.2', native: true, required: get_option('man-pages'))
if scdoc.found()
	scdoc_prog = find_program(scdoc.get_variable(pkgconfig: 'scdoc'), native: true)
	mandir = get_option('mandir')
	man_files = [
		'sway/sway.1.scd',
		'sway/sway.5.scd',
		'sway/sway-bar.5.scd',
		'sway/sway-input.5.scd',
		'sway/sway-ipc.7.scd',
		'sway/sway-output.5.scd',
		'swaybar/swaybar-protocol.7.scd',
		'swaymsg/swaymsg.1.scd',
	]

	if get_option('swaynag')
		man_files += [
			'swaynag/swaynag.1.scd',
			'swaynag/swaynag.5.scd',
		]
	endif

	foreach filename : man_files
		topic = filename.split('.')[-3].split('/')[-1]
		section = filename.split('.')[-2]
		output = '@0@.@1@'.format(topic, section)

		custom_target(
			output,
			input: filename,
			output: output,
			command: scdoc_prog,
			install: true,
			feed: true,
			capture: true,
			install_dir: '@0@/man@1@'.format(mandir, section)
		)
	endforeach
endif

add_project_arguments('-DSYSCONFDIR="/@0@"'.format(join_paths(prefix, sysconfdir)), language : 'c')

version = '"@0@"'.format(meson.project_version())
git = find_program('git', native: true, required: false)
if git.found()
	git_commit = run_command([git, 'rev-parse', '--short', 'HEAD'], check: false)
	git_branch = run_command([git, 'rev-parse', '--abbrev-ref', 'HEAD'], check: false)
	if git_commit.returncode() == 0 and git_branch.returncode() == 0
		version = '"@0@-@1@ (" __DATE__ ", branch \'@2@\')"'.format(
			meson.project_version(),
			git_commit.stdout().strip(),
			git_branch.stdout().strip(),
		)
	endif
endif
add_project_arguments('-DSWAY_VERSION=@0@'.format(version), language: 'c')

# Compute the relative path used by compiler invocations.
source_root = meson.current_source_dir().split('/')
build_root = meson.global_build_root().split('/')
relative_dir_parts = []
i = 0
in_prefix = true
foreach p : build_root
	if i >= source_root.length() or not in_prefix or p != source_root[i]
		in_prefix = false
		relative_dir_parts += '..'
	endif
	i += 1
endforeach
i = 0
in_prefix = true
foreach p : source_root
	if i >= build_root.length() or not in_prefix or build_root[i] != p
		in_prefix = false
		relative_dir_parts += p
	endif
	i += 1
endforeach
relative_dir = join_paths(relative_dir_parts) + '/'

# Strip relative path prefixes from the code if possible, otherwise hide them.
if cc.has_argument('-fmacro-prefix-map=/prefix/to/hide=')
	add_project_arguments(
		'-fmacro-prefix-map=@0@='.format(relative_dir),
		language: 'c',
	)
else
	add_project_arguments(
		'-DSWAY_REL_SRC_DIR="@0@"'.format(relative_dir),
		language: 'c',
	)
endif


sway_inc = include_directories('include')

subdir('include')
subdir('protocols')
subdir('common')
subdir('sway')
subdir('swaymsg')

if get_option('swaybar') or get_option('swaynag')
	subdir('client')
endif
if get_option('swaybar')
	subdir('swaybar')
endif
if get_option('swaynag')
	subdir('swaynag')
endif

config = configuration_data()
config.set('datadir', join_paths(prefix, datadir))
config.set('prefix', prefix)
config.set('sysconfdir', join_paths(prefix, sysconfdir))

configure_file(
	configuration: config,
	input: 'config.in',
	output: '@BASENAME@',
	install_dir: join_paths(sysconfdir, 'sway')
)

install_data(
	'sway.desktop',
	install_dir: join_paths(datadir, 'wayland-sessions')
)

if get_option('default-wallpaper')
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
	wallpaper_install_dir = join_paths(datadir, 'backgrounds', 'sway')

	install_data(wallpaper_files, install_dir: wallpaper_install_dir)
endif

subdir('completions')

summary({
	'xwayland': have_xwayland,
	'gdk-pixbuf': gdk_pixbuf.found(),
	'tray': have_tray,
	'man-pages': scdoc.found(),
}, bool_yn: true)
