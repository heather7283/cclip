project(
    'cclip', 'c',
    version: '1.0.0',
    license: 'GPL-3.0-or-later',
    default_options: ['buildtype=release', 'warning_level=3']
)

# supported by both clang and gcc and makes my life easier
add_project_arguments('-Wno-gnu-zero-variadic-macro-arguments', language: 'c')

add_project_arguments('-DCCLIP_VERSION="@0@"'.format(meson.project_version()), language: 'c')

wayland_scanner = find_program('wayland-scanner')

sqlite3_dep = dependency('sqlite3')
wayland_client_dep = dependency('wayland-client')

if get_option('man')
  scdoc = find_program('scdoc')

  foreach man_src : [{'name': 'cclipd', 'section': 1},
                     {'name': 'cclip', 'section': 1}]
      name = man_src['name']
      section = man_src['section']

      in_file = join_paths('man', '@0@.@1@.scd'.format(name, section))
      out_file = '@0@.@1@'.format(name, section)

      custom_target(out_file,
          output: out_file,
          input: in_file,
          command: [ scdoc ],
          capture: true,
          feed: true,
          install: true,
          install_dir: join_paths(get_option('mandir'), 'man@0@'.format(section))
      )
  endforeach
endif

private_code = custom_target('private_code',
    output: 'wlr-data-control-unstable-v1.c',
    input: 'protocol/wlr-data-control-unstable-v1.xml',
    command: [ wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@' ]
)

client_header = custom_target('client_header',
    output: 'wlr-data-control-unstable-v1-client-protocol.h',
    input: 'protocol/wlr-data-control-unstable-v1.xml',
    command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ]
)

executable('cclip',
    'cclip.c',
    'db.c',
    'config.c',
    dependencies: [ sqlite3_dep ],
    install: true
)

executable('cclipd',
    'cclipd.c',
    'db.c',
    'wayland.c',
    'config.c',
    private_code,
    client_header,
    dependencies: [ sqlite3_dep, wayland_client_dep ],
    install: true
)

