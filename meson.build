project('vaulthalla', 'cpp',
        version: '0.8.3',
        license: 'AGPLv3',
        default_options: ['warning_level=3', 'cpp_std=c++23'])

# ╭──────────────────── DEBUG FLAGS ─────────────────────╮
if get_option('buildtype') == 'debug'
    add_project_arguments('-fsanitize=address,undefined', language: 'cpp')
    add_project_link_arguments('-fsanitize=address,undefined', language: 'cpp')
endif

# ╭──────────────────── DEFINE DEPENDENCIES ────────────────────╮

hash_deps = [
    dependency('libsodium', required: true),
    dependency('libcurl', required: true)
]

deps = hash_deps + [
    dependency('boost', modules: ['filesystem', 'system'], required: true),
    dependency('libpqxx', required: true),
    dependency('libpdfium', required: true),
    dependency('libmagic', required: true),
    dependency('fuse3', required: true),
    dependency('yaml-cpp', required: true),
    dependency('libjpeg', required: true),
    dependency('pugixml', required: true),
    dependency('uuid', required: true),
    dependency('threads', required: true),
    dependency('spdlog', required: true),
    dependency('libturbojpeg', required: true),
    dependency('tss2-esys', required: true),
    dependency('tss2-tctildr', required: true),
    dependency('openssl', version: '>=3.0', required: true),
    dependency('zlib', required: true)
]

test_deps = deps + dependency('gtest', required: true, main: true)

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
           dependencies: hash_deps,
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
