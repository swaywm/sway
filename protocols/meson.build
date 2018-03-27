wl_protocol_dir = wayland_protos.get_pkgconfig_variable('pkgdatadir')

wayland_scanner = find_program('wayland-scanner')

wayland_scanner_server = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.h',
	arguments: ['server-header', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: ['code', '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

protocols = []

client_protocols = [
	'wlr-layer-shell-unstable-v1.xml',
]

wl_protos_src = []
wl_protos_headers = []

foreach p : protocols
	xml = join_paths(p)
	wl_protos_src += wayland_scanner_code.process(xml)
	wl_protos_headers += wayland_scanner_server.process(xml)
endforeach

foreach p : client_protocols
	xml = join_paths(p)
	wl_protos_headers += wayland_scanner_client.process(xml)
endforeach

lib_wl_protos = static_library('wl_protos', wl_protos_src + wl_protos_headers,
	dependencies: [wayland_client]) # for the include directory

sway_protos = declare_dependency(
	link_with: lib_wl_protos,
	sources: wl_protos_headers,
)
