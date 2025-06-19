project('vaulthalla', 'cpp',
        version : '0.0.1',
        default_options : ['warning_level=3', 'cpp_std=c++2b'])

if get_option('buildtype') == 'debug'
    add_project_arguments('-fsanitize=undefined', language: 'cpp')
    add_project_link_arguments('-fsanitize=undefined', language: 'cpp')
endif

boost_dep = dependency('boost', modules: ['filesystem', 'system'], required: true)
nlohmann_json_dep = dependency('nlohmann_json', required: true)
libpqxx_dep = dependency('libpqxx', required: true)
libsodium_dep = dependency('libsodium', required: true)
jwt_cpp_dep = dependency('jwt-cpp', required: true)
libcurl_dep = dependency('libcurl', required: true)
libfuse_dep = dependency('fuse3', required: true)
uuid_dep = dependency('uuid', required: true)
thread_dep = dependency('threads', required: true)
gtest_dep = dependency('gtest', required: true, main: true)

source_root = '.'
shared_root = source_root + '/shared/src'
project_root = source_root + '/src'
fuse_root = source_root + '/fuse_daemon/src'
tests_source_root = source_root + '/tests'
main = files(source_root + '/main.cpp')

global_deps = [boost_dep, nlohmann_json_dep, libpqxx_dep, libsodium_dep, jwt_cpp_dep, libcurl_dep, uuid_dep, thread_dep]
fuse_deps = [libfuse_dep, thread_dep, uuid_dep]
test_deps = global_deps + gtest_dep

inc = include_directories(source_root + '/include', source_root + '/shared/include')
fuse_inc = include_directories(source_root + '/fuse_daemon/include', source_root + '/shared/include')

shared_src_files = []
foreach cpp_file : run_command('find', shared_root, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    shared_src_files += files(cpp_file)
endforeach

src_files = shared_src_files
foreach cpp_file : run_command('find', project_root, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    src_files += files(cpp_file)
endforeach

fuse_src_files = shared_src_files
foreach cpp_file : run_command('find', fuse_root, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    fuse_src_files += files(cpp_file)
endforeach

core_test_sources = src_files
foreach cpp_file : run_command('find', tests_source_root + '/core', '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    core_test_sources += files(cpp_file)
endforeach

cloud_test_sources = src_files
foreach cpp_file : run_command('find', tests_source_root + '/cloud', '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    cloud_test_sources += files(cpp_file)
endforeach

src_files += main

vaulthalla = executable('vaulthalla',
                        src_files,
                        dependencies: global_deps,
                        include_directories: inc,
                        install : true)

vaulthalla_fuse = executable('vaulthalla_fuse',
                        fuse_src_files,
                        dependencies: fuse_deps,
                        include_directories: fuse_inc,
                        install : true)

test('test_core', executable('test_core',
                        core_test_sources,
                        dependencies: test_deps,
                        include_directories: inc,
                        install : true))

test('test_cloud', executable('test_cloud',
                        cloud_test_sources,
                        dependencies: test_deps,
                        include_directories: inc,
                        install : true))
