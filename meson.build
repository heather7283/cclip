project(
    'cclip', 'c',
    version: '3.0.0-next',
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

private_code = custom_target('wlr-data-control-unstable-v1-private-code',
    output: 'wlr-data-control-unstable-v1.c',
    input: 'protocol/wlr-data-control-unstable-v1.xml',
    command: [ wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@' ]
)

client_header = custom_target('wlr-data-control-unstable-v1-client-header',
    output: 'wlr-data-control-unstable-v1.h',
    input: 'protocol/wlr-data-control-unstable-v1.xml',
    command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ]
)

xxhash_lib = static_library('xxhash', 'thirdparty/xxhash/xxhash.c')

include_dirs = [
  'thirdparty/xxhash',
  'thirdparty/getopt',
  'src/common'
]

executable('cclip',
    'src/cclip/cclip.c',
    'src/cclip/utils.c',
    'src/cclip/action_list.c',
    'src/cclip/action_get.c',
    'src/cclip/action_tag.c',
    'src/cclip/action_delete.c',
    'src/cclip/action_wipe.c',
    'src/cclip/action_vacuum.c',
    'src/common/xmalloc.c',
    'src/common/db_path.c',
    'thirdparty/getopt/getopt.c',
    include_directories: include_dirs,
    dependencies: [ sqlite3_dep ],
    install: true
)

executable('cclipd',
    'src/cclipd/cclipd.c',
    'src/cclipd/db.c',
    'src/cclipd/wayland.c',
    'src/cclipd/preview.c',
    'src/cclipd/config.c',
    'src/cclipd/log.c',
    'src/common/xmalloc.c',
    'src/common/db_path.c',
    'thirdparty/getopt/getopt.c',
    private_code,
    client_header,
    include_directories: include_dirs,
    link_with: [ xxhash_lib ],
    dependencies: [ sqlite3_dep, wayland_client_dep ],
    install: true
)

