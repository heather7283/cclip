project(
    'cclip', 'c',
    version: '2.0.0',
    license: 'GPL-3.0-or-later',
    default_options: ['warning_level=3']
)

# supported by both clang and gcc and makes my life easier
add_project_arguments('-Wno-gnu-zero-variadic-macro-arguments', language: 'c')
add_project_arguments('-Wno-unused-parameter', language: 'c')

# version, branch and commit info
git_hash = run_command('git', 'rev-parse', 'HEAD', check: false).stdout().strip()
if git_hash == ''
    git_hash = 'unknown'
endif
add_project_arguments('-DCCLIP_GIT_COMMIT_HASH="@0@"'.format(git_hash), language: 'c')

git_tag = run_command('git', 'describe', '--tags', check: false).stdout().strip()
if git_tag == ''
    git_tag = 'unknown'
endif
add_project_arguments('-DCCLIP_GIT_TAG="@0@"'.format(git_tag), language: 'c')

git_branch = run_command('git', 'branch', '--show-current', check: false).stdout().strip()
if git_branch == ''
    git_branch = 'unknown'
endif
add_project_arguments('-DCCLIP_GIT_BRANCH="@0@"'.format(git_branch), language: 'c')

wayland_scanner = find_program('wayland-scanner')

static_link = get_option('static')
if static_link
    add_project_link_arguments('-static', language: 'c')
endif

sqlite3_dep = dependency('sqlite3', static: static_link)
wayland_client_dep = dependency('wayland-client', static: static_link)

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
    'src/cclip.c',
    'src/db.c',
    'src/xmalloc.c',
    dependencies: [ sqlite3_dep ],
    install: true
)

executable('cclipd',
    'src/cclipd.c',
    'src/db.c',
    'src/wayland.c',
    'src/preview.c',
    'src/config.c',
    'src/xmalloc.c',
    private_code,
    client_header,
    dependencies: [ sqlite3_dep, wayland_client_dep ],
    install: true
)

