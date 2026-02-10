project(
    'cclip', 'c',
    version: '3.2.1',
    license: 'GPL-3.0-or-later',
    default_options: ['warning_level=3']
)

cc = meson.get_compiler('c')
if cc.get_id() != 'gcc'
  add_project_arguments('-Wno-gnu-zero-variadic-macro-arguments', language: 'c')
endif
add_project_arguments('-Wno-unused-parameter', language: 'c')
add_project_arguments('-Wno-pedantic', language: 'c')

# version, branch and commit info
git_hash = run_command('git', 'rev-parse', 'HEAD', check: false).stdout().strip()
if git_hash == ''
    git_hash = 'unknown'
endif
add_project_arguments('-DCCLIP_GIT_COMMIT_HASH="@0@"'.format(git_hash), language: 'c')

git_tag = run_command('git', 'describe', '--tags', check: false).stdout().strip()
if git_tag == ''
    git_tag = meson.project_version()
endif
add_project_arguments('-DCCLIP_GIT_TAG="@0@"'.format(git_tag), language: 'c')

git_branch = run_command('git', 'branch', '--show-current', check: false).stdout().strip()
if git_branch == ''
    git_branch = 'unknown'
endif
add_project_arguments('-DCCLIP_GIT_BRANCH="@0@"'.format(git_branch), language: 'c')

static_link = get_option('static')
if static_link
    add_project_link_arguments('-static', language: 'c')
endif

sqlite3_dep = dependency('sqlite3', static: static_link)
wayland_client_dep = dependency('wayland-client', static: static_link)

if get_option('man')
    subdir('man')
endif

subdir('protocol')

xxhash_lib = static_library('xxhash', 'thirdparty/xxhash/xxhash.c')
getopt_lib = static_library('getopt', 'thirdparty/getopt/getopt.c')

include_dirs = include_directories([
    'thirdparty/xxhash',
    'thirdparty/getopt',
    'thirdparty/pollen',
    'src/common',
    'src'
])

common_sources = files([
    'src/common/log.c',
    'src/common/xmalloc.c',
    'src/common/db.c',
    'src/collections/string.c',
    'src/collections/vec.c',
])

cclip_sources = files([
    'src/cclip/cclip.c',
    'src/cclip/utils.c',
    'src/cclip/actions/actions.c',
    'src/cclip/actions/list.c',
    'src/cclip/actions/get.c',
    'src/cclip/actions/tag.c',
    'src/cclip/actions/tags.c',
    'src/cclip/actions/delete.c',
    'src/cclip/actions/wipe.c',
    'src/cclip/actions/vacuum.c',
    'src/cclip/actions/copy.c',
])

cclipd_sources = files([
    'src/cclipd/cclipd.c',
    'src/cclipd/sql.c',
    'src/cclipd/wayland.c',
    'src/cclipd/preview.c',
    'src/cclipd/config.c',
    'src/cclipd/eventloop.c',
])

executable('cclip', cclip_sources + common_sources + protocol_sources,
    include_directories: include_dirs,
    link_with: [ getopt_lib ],
    dependencies: [ sqlite3_dep, wayland_client_dep ],
    install: true
)

executable('cclipd', cclipd_sources + common_sources + protocol_sources,
    include_directories: include_dirs,
    link_with: [ xxhash_lib, getopt_lib ],
    dependencies: [ sqlite3_dep, wayland_client_dep ],
    install: true
)

