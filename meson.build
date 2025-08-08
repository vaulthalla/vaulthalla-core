project('vaulthalla', 'cpp',
        version: '0.8.8',
        license: 'AGPLv3',
        default_options: [
            'warning_level=3',
            'cpp_std=c++23',
        ])

if get_option('buildtype') == 'debug'
    add_project_arguments(['-g', '-O0', '-fsanitize=address,undefined'], language: 'cpp')
    add_project_link_arguments(['-g', '-O0', '-fsanitize=address,undefined'], language: 'cpp')

elif get_option('buildtype') == 'release'
    add_project_arguments(['-O3', '-DNDEBUG'], language: 'cpp')
    add_project_link_arguments(['-O3'], language: 'cpp')
    # lower warnings in release
    add_project_arguments('-Wno-unused-variable', language: 'cpp')  # optional

else
    add_project_arguments(['-O2', '-DNDEBUG'], language: 'cpp')
    add_project_link_arguments(['-O2'], language: 'cpp')
endif


# ╭──────────────────── DEFINE DEPENDENCIES ────────────────────╮

deps = [
    dependency('libsodium', required: true),
    dependency('libcurl', required: true),
    dependency('boost', modules: ['filesystem', 'system'], required: true),
    dependency('libpqxx', required: true),
    dependency('pdfium', required: true),
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

# ╭──────────────────── INCLUDE DIRS ─────────────────────╮

inc       = include_directories('include', 'vendor')


# ╭──────────────────── SOURCE COLLECTION ───────────────╮

sources = []
foreach f : run_command('find', src, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    sources += files(f)
endforeach


# ╭──────────────────── LIBRARIES ───────────────────────╮

lib_core = static_library('core', sources,
                     include_directories: inc,
                     dependencies: deps)

lib_seed = static_library('seed', files(join_paths(source_root, 'seed/src/seed_db.cpp')),
    include_directories: include_directories('seed/include', 'include', 'vendor'),
    link_with: lib_core)


# ╭──────────────────── DEPENDENCIES ────────────────────╮

dep = declare_dependency(
    include_directories: inc,
    link_with: lib_core,
    dependencies: deps)


# ╭──────────────────── EXECUTABLES ─────────────────────╮

executable('vaulthalla', main,
           link_with: [lib_seed],
           include_directories: inc,
           dependencies: dep,
           install: true)


# ╭──────────────────── INSTALLATION ────────────────────╮

install_data('deploy/config/config.yaml', install_dir: join_paths(get_option('sysconfdir'), 'vaulthalla'))
install_data('deploy/config/config_template.yaml.in', install_dir: join_paths(get_option('sysconfdir'), 'vaulthalla'))
install_data('deploy/systemd/vaulthalla.service', install_dir: get_option('systemd_system_unit_dir'))


# ╭──────────────────── TESTS ───────────────────────────╮

configure_file(input: 'test-assets/sample.jpg', output: 'sample.jpg', copy: true)
configure_file(input: 'test-assets/sample.pdf', output: 'sample.pdf', copy: true)
configure_file(
    input: 'test-assets/Screenshot 2025-06-26 at 3.29.35\u202FPM.png',
    output: 'Screenshot 2025-06-26 at 3.29.35\u202FPM.png',
    copy: true
)

all_test_sources = sources
foreach f : run_command('find', tests_src, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    all_test_sources += files(f)
endforeach

test('test_all',
     executable('test_all', all_test_sources,
                link_with: [lib_core, lib_seed],
                include_directories: inc,
                dependencies: test_deps))
