project('vaulthalla', 'cpp',
        version: '0.7.0',
        license: 'AGPLv3',
        default_options: ['warning_level=3', 'cpp_std=c++23'])

# ╭──────────────────── DEBUG FLAGS ─────────────────────╮
if get_option('buildtype') == 'debug'
    add_project_arguments('-fsanitize=address,undefined', language: 'cpp')
    add_project_link_arguments('-fsanitize=address,undefined', language: 'cpp')
endif

add_project_link_arguments('-lcrypto', language: 'cpp')
add_project_link_arguments('-lz', language: 'cpp')
add_project_link_arguments('-lturbojpeg', language: 'cpp')

# ╭──────────────────── DEPENDENCIES ────────────────────╮
boost_dep       = dependency('boost', modules: ['filesystem', 'system'], required: true)
libpqxx         = dependency('libpqxx', required: true)
pdfium          = dependency('libpdfium', required: true)
libsodium       = dependency('libsodium', required: true)
libmagic        = dependency('libmagic', required: true)
libcurl         = dependency('libcurl', required: true)
libfuse         = dependency('fuse3', required: true)
yaml_cpp        = dependency('yaml-cpp', required: true)
libjpeg         = dependency('libjpeg', required: true)
pugixml         = dependency('pugixml', required: true)
uuid            = dependency('uuid', required: true)
thread_dep      = dependency('threads', required: true)
gtest           = dependency('gtest', required: true, main: true)

global_deps     = [boost_dep, libpqxx, yaml_cpp, libmagic, uuid, pugixml, thread_dep, libsodium, libcurl, libjpeg, pdfium]
core_deps       = global_deps
fuse_deps       = global_deps + [libfuse]
test_deps       = fuse_deps + [gtest]

# ╭──────────────────── SOURCE DIRS ─────────────────────╮
source_root = meson.project_source_root()
core_src_dir    = join_paths(source_root, 'core_daemon/src')
fuse_src_dir    = join_paths(source_root, 'fuse_daemon/src')
shared_src_dir  = join_paths(source_root, 'shared/src')
tests_src_dir   = join_paths(source_root, 'tests')

core_main       = files(join_paths(source_root, 'main.cpp'))
fuse_main       = files(join_paths(source_root, 'fuse_daemon/src/main.cpp'))

hash_pass_src   = files(
    join_paths(core_src_dir, 'auth/PasswordHash.cpp'),
    join_paths(core_src_dir, 'auth/PasswordUtils.cpp')
)
hash_pass_main  = files(join_paths(source_root, 'deploy/hash_password.cpp'))

# ╭──────────────────── INCLUDE DIRS ─────────────────────╮
shared_inc_paths = ['shared/include', 'vendor']
core_inc_paths = shared_inc_paths + ['core_daemon/include']
fuse_inc_paths = shared_inc_paths + ['fuse_daemon/include']

core_inc       = include_directories(core_inc_paths)
fuse_inc       = include_directories(fuse_inc_paths)
hash_pass_inc  = include_directories('core_daemon/include', 'shared/include')

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


# ╭──────────────────── LIBRARIES ───────────────────────╮
core_lib = static_library('core', core_sources,
                          include_directories: core_inc,
                          dependencies: core_deps)

fuse_lib = static_library('fuse', fuse_sources,
                          include_directories: fuse_inc,
                          dependencies: fuse_deps)


# ╭──────────────────── EXECUTABLES ─────────────────────╮

executable('core_daemon', core_main,
           link_with: core_lib,
           include_directories: core_inc,
           dependencies: core_deps)

executable('fuse_daemon', fuse_main,
           link_with: fuse_lib,
           include_directories: fuse_inc,
           dependencies: fuse_deps)

executable('hash_password',
           [hash_pass_src, hash_pass_main],
           dependencies: [libsodium, libcurl],
           include_directories: hash_pass_inc,
           install: true)


# ╭──────────────────── TESTS ───────────────────────────╮
test_assets = files(
    'test-assets/sample.jpg',
    'test-assets/sample.pdf',
    'test-assets/Screenshot 2025-06-26 at 3.29.35\u202FPM.png')

foreach asset : test_assets
    asset_path = '@0@'.format(asset)
    asset_name = asset_path.split('/')[-1]

    configure_file(
        input: asset,
        output: asset_name,
        copy: true
    )
endforeach


# === 🧪 Cloud-Side Tests ===
test_sources = fuse_sources
foreach f : run_command('find', tests_src_dir + '/cloud', '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    test_sources += files(f)
endforeach

test('test_cloud',
     executable('test_cloud', test_sources,
                link_with: [core_lib, fuse_lib],
                include_directories: fuse_inc,
                dependencies: test_deps))
