project('vaulthalla', 'cpp',
        version: '0.9.3',
        license: 'AGPL-3.0-or-later',
        license_files: ['LICENSE', 'debian/copyright'],
        default_options: [
            'warning_level=3',
            'cpp_std=c++23',
            'unity=on',
            'unity_size=8'
        ]
)

# ╭──────────────────── VERSION HEADER ─────────────────────╮

version_split = meson.project_version().split('.')
if version_split.length() < 3
    error('Project version must be in the format X.Y.Z (e.g., 0.9.2)')
endif

conf = configuration_data()
conf.set_quoted('VH_VERSION', meson.project_version())
conf.set_quoted('VH_VERSION_MAJOR', version_split[0])
conf.set_quoted('VH_VERSION_MINOR', version_split[1])
conf.set_quoted('VH_VERSION_PATCH', version_split[2])

vh_version_header = configure_file(
    output: 'version.h',
    configuration: conf,
    install: true,
    install_dir: get_option('includedir') / 'vaulthalla'
)

lib_version = static_library('vaulthalla-version', include_directories: '.')


# ╭──────────────────── DEFINE DEPENDENCIES ────────────────────╮

deps = [
    dependency('fuse3', required: true),
    dependency('libsodium', required: true),
    dependency('libcurl', required: true),
    dependency('boost', modules: ['filesystem', 'system'], required: true),
    dependency('libpqxx', required: true),
    dependency('pdfium', required: true),
    dependency('libmagic', required: true),
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
    dependency('zlib', required: true),
    dependency('fmt', required: true)
]

test_deps = deps + dependency('gtest', required: true, main: true)


# ╭────────────────────   FILES   ──────────────────────╮

inc         = include_directories('include', 'vendor')
source_root = meson.project_source_root()
src         = join_paths(source_root, 'src')
tests_src   = join_paths(source_root, 'tests')
main        = files(join_paths(source_root, 'main.cpp'))

sources = []
foreach f : run_command('find', src, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    sources += files(f)
endforeach


# ╭──────────────────── LIBRARIES ───────────────────────╮

lib_core = static_library('vaulthalla', sources, include_directories: inc, dependencies: deps)

lib_seed = static_library('vhseed', files(join_paths(source_root, 'seed/src/seed_db.cpp')),
    include_directories: include_directories('seed/include', 'include', 'vendor'),
    link_with: lib_core)


# ╭─────────────────────────────────── DEPENDENCIES ───────────────────────────────────╮

dep = declare_dependency(
    include_directories: inc,
    link_with: [lib_core, lib_seed, lib_version],
    dependencies: deps)


# ╭─────────────────────────────────── EXECUTABLES ────────────────────────────────────╮

executable('vaulthalla-server', main,
           include_directories: inc,
           dependencies: dep,
           install: true)

executable('vaulthalla-cli', files(join_paths(source_root, 'cli_main.cpp')),
           include_directories: include_directories('vendor'),
           dependencies: [dependency('fmt', required: true)],
           install: true)


# ╭─────────────────────────────────── INSTALLATION ───────────────────────────────────╮

license_dir = join_paths(get_option('prefix'), get_option('datadir'), 'doc', 'vaulthalla')
install_data('LICENSE', install_dir: license_dir)
install_data('debian/copyright', install_dir: license_dir, rename: 'copyright')

install_dir = join_paths(get_option('prefix'), get_option('bindir'))
install_symlink('vaulthalla', install_dir: install_dir, pointing_to: 'vaulthalla-cli')
install_symlink('vh', install_dir: install_dir, pointing_to: 'vaulthalla-cli')

install_data('deploy/config/config.yaml', install_dir: join_paths(get_option('sysconfdir'), 'vaulthalla'))
install_data('deploy/config/config_template.yaml.in', install_dir: join_paths(get_option('sysconfdir'), 'vaulthalla'))
install_data('deploy/systemd/vaulthalla-cli.socket', install_dir: get_option('systemd_system_unit_dir'))

configure_file(
    input: 'deploy/systemd/vaulthalla.service.in',
    output: 'vaulthalla.service',
    configuration: { 'BINDIR': install_dir },
    install_dir: get_option('systemd_system_unit_dir')
)

configure_file(
    input: 'deploy/systemd/vaulthalla-cli.service.in',
    output: 'vaulthalla-cli.service',
    configuration: { 'BINDIR': install_dir },
    install_dir: get_option('systemd_system_unit_dir')
)


# ╭────────────────────────────────────── TESTS ────────────────────────────────────────╮

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
