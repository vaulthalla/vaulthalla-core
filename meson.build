project('vaulthalla', 'cpp',
        version : '0.0.1',
        default_options : ['warning_level=3', 'cpp_std=c++2a'])

boost_dep = dependency('boost', modules: ['filesystem', 'system'], required: true)
nlohmann_json_dep = dependency('nlohmann_json', required: true)
libpqxx_dep = dependency('libpqxx', required: true)
libsodium_dep = dependency('libsodium', required: true)
jwt_cpp_dep = dependency('jwt-cpp', required: true)
libcurl_dep = dependency('libcurl', required: true)
thread_dep = dependency('threads', required: true)
gtest_dep = dependency('gtest', required: true, main: true)

source_root = '.'
project_root = source_root + '/src'
tests_source_root = source_root + '/tests'
main = files(source_root + '/main.cpp')

global_deps = [boost_dep, nlohmann_json_dep, libpqxx_dep, libsodium_dep, jwt_cpp_dep, libcurl_dep, thread_dep]
test_deps = global_deps + gtest_dep

inc = include_directories(source_root + '/include')

src_files = []
foreach cpp_file : run_command('find', project_root, '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    src_files += files(cpp_file)
endforeach

core_test_sources = src_files
foreach cpp_file : run_command('find', tests_source_root + '/core', '-type', 'f', '-name', '*.cpp', check: true).stdout().strip().split('\n')
    core_test_sources += files(cpp_file)
endforeach

src_files += main

vaulthalla = executable('vaulthalla',
                        src_files,
                        dependencies: global_deps,
                        include_directories: inc,
                        install : true)

test('test_core', executable('test_core',
                        core_test_sources,
                        dependencies: test_deps,
                        include_directories: inc,
                        install : true))
