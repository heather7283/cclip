wayland_scanner = find_program('wayland-scanner')

private_code = custom_target('wlr-data-control-unstable-v1-private-code',
    output: 'wlr-data-control-unstable-v1.c',
    input: 'wlr-data-control-unstable-v1.xml',
    command: [ wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@' ]
)

client_header = custom_target('wlr-data-control-unstable-v1-client-header',
    output: 'wlr-data-control-unstable-v1.h',
    input: 'wlr-data-control-unstable-v1.xml',
    command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ]
)

proto_sources = [ private_code, client_header ]

