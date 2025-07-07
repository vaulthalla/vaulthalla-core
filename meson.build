project('vaulthalla', 'cpp',
        version: '0.5.0',
        default_options: ['warning_level=3', 'cpp_std=c++2b'])

# ╭──────────────────── DEBUG FLAGS ─────────────────────╮
if get_option('buildtype') == 'debug'
    add_project_arguments('-fsanitize=undefined', language: 'cpp')
    add_project_link_arguments('-fsanitize=undefined', language: 'cpp')
endif

# ╭──────────────────── DEPENDENCIES ────────────────────╮
boost_dep       = dependency('boost', modules: ['filesystem', 'system'], required: true)
nlohmann_json   = dependency('nlohmann_json', required: true)
libpqxx         = dependency('libpqxx', required: true)
libsodium       = dependency('libsodium', required: true)
libmagic        = dependency('libmagic', required: true)
jwt_cpp         = dependency('jwt-cpp', required: true)
libcurl         = dependency('libcurl', required: true)
libfuse         = dependency('fuse3', required: true)
yaml_cpp        = dependency('yaml-cpp', required: true)
uuid            = dependency('uuid', required: true)
thread_dep      = dependency('threads', required: true)
gtest           = dependency('gtest', required: true, main: true)

global_deps     = [boost_dep, nlohmann_json, libpqxx, yaml_cpp, libmagic, uuid, thread_dep]
core_deps       = global_deps + [libsodium, jwt_cpp, libcurl]
fuse_deps       = global_deps + [libfuse]
test_deps       = core_deps + [gtest]

# ╭──────────────────── SOURCE DIRS ─────────────────────╮
source_root         = '.'
core_src_dir        = source_root + '/core_daemon/src'
fuse_src_dir        = source_root + '/fuse_daemon/src'
shared_src_dir      = source_root + '/shared/src'
tests_src_dir       = source_root + '/tests'
core_main           = files(source_root + '/main.cpp')
hash_pass_src       = files(core_src_dir + '/crypto/PasswordHash.cpp')
hash_pass_main      = files(source_root + '/deploy/psql/hash_password.cpp')

core_inc            = include_directories('core_daemon/include', 'shared/include')
fuse_inc            = include_directories('fuse_daemon/include', 'shared/include')
hash_pass_inc       = include_directories('core_daemon/include')

# ╭──────────────────── SOURCE COLLECTION ───────────────╮
shared_sources = []
foreach f : run_command('find', shared_src_dir, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    shared_sources += files(f)
endforeach

core_sources = shared_sources
foreach f : run_command('find', core_src_dir, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    core_sources += files(f)
endforeach

fuse_sources = shared_sources
foreach f : run_command('find', fuse_src_dir, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    fuse_sources += files(f)
endforeach

# Core Daemon: add the entrypoint
core_sources += core_main

# ╭──────────────────── EXECUTABLES ─────────────────────╮

# === ⚙️ Core Daemon ===
executable('core_daemon',
           core_sources,
           dependencies: core_deps,
           include_directories: core_inc,
           install: true
)

# === 🧷 FUSE Daemon ===
executable('fuse_daemon',
           fuse_sources,
           dependencies: fuse_deps,
           include_directories: fuse_inc,
           install: true
)

executable('hash_password',
           [hash_pass_src, hash_pass_main],
           dependencies: libsodium,
           include_directories: hash_pass_inc,
           install: true
)

# === 🧪 Cloud-Side Tests ===
cloud_test_sources = core_sources
foreach f : run_command('find', tests_src_dir + '/cloud', '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    cloud_test_sources += files(f)
endforeach

test('test_cloud', executable('test_cloud',
                              cloud_test_sources,
                              dependencies: test_deps,
                              include_directories: core_inc,
                              install: true
     ))
