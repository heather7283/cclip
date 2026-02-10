wayland_scanner = find_program('wayland-scanner')

protocols = [
  'wlr-data-control-unstable-v1',
]

protocol_sources = []

foreach protocol : protocols
  in_file = '@0@.xml'.format(protocol)
  out_h = '@0@.h'.format(protocol)
  out_c = '@0@.c'.format(protocol)

  header = custom_target('@0@-client-header'.format(protocol),
                         output: out_h,
                         input: in_file,
                         command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ])

  protocol_sources += [ header ]

  source = custom_target('@0@-private-code'.format(protocol),
                         output: out_c,
                         input: in_file,
                         command: [ wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@' ])

  protocol_sources += [ source ]
endforeach

