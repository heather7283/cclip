project(
    'cclip', 'c',
    version: '2.0.0-next',
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
    subdir('man')
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

xxhash_lib = static_library('xxhash', 'thirdparty/xxhash/xxhash.c')

executable('cclip',
    'src/cclip.c',
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
    include_directories: [ 'thirdparty/xxhash' ],
    link_with: [ xxhash_lib ],
    dependencies: [ sqlite3_dep, wayland_client_dep ],
    install: true
)

