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

# ╭──────────────────── DEFINE DEPENDENCIES ────────────────────╮
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

deps = [libfuse, boost_dep, libpqxx, yaml_cpp, libmagic, uuid, pugixml, thread_dep, libsodium, libcurl, libjpeg, pdfium]
test_deps = deps + [gtest]

# ╭──────────────────── SOURCE DIRS ─────────────────────╮

source_root = meson.project_source_root()
src         = join_paths(source_root, 'src')
tests_src   = join_paths(source_root, 'tests')
main        = files(join_paths(source_root, 'main.cpp'))

hash_pass_src   = files(
    join_paths(src, 'crypto/PasswordHash.cpp'),
    join_paths(src, 'crypto/PasswordUtils.cpp')
)
hash_pass_main  = files(join_paths(source_root, 'deploy/hash_password.cpp'))


# ╭──────────────────── INCLUDE DIRS ─────────────────────╮

inc       = include_directories('include', 'vendor')
hash_pass_inc  = include_directories('include')


# ╭──────────────────── SOURCE COLLECTION ───────────────╮

sources = []
foreach f : run_command('find', src, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    sources += files(f)
endforeach


# ╭──────────────────── LIBRARIES ───────────────────────╮

lib = static_library('core', sources,
                     include_directories: inc,
                     dependencies: deps)


# ╭──────────────────── DEPENDENCIES ────────────────────╮

dep = declare_dependency(
    include_directories: inc,
    link_with: lib,
    dependencies: deps)


# ╭──────────────────── EXECUTABLES ─────────────────────╮

executable('vaulthalla', main,
           link_with: lib,
           include_directories: inc,
           dependencies: dep)

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

test('test_cloud',
     executable('test_cloud', sources + files(join_paths(tests_src, 'test_S3Provider.cpp')),
                link_with: [lib],
                include_directories: inc,
                dependencies: test_deps))

test('test_path',
     executable('test_path', sources + files(join_paths(tests_src, 'test_Path.cpp')),
                link_with: [lib],
                include_directories: inc,
                dependencies: test_deps))
