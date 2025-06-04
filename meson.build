project('vaulthalla', 'cpp',
        version : '0.0.1',
        default_options : ['warning_level=3', 'cpp_std=c++2a'])

boost_dep = dependency('boost', modules: ['filesystem', 'system', 'serialization'], required: true)
nlohmann_json_dep = dependency('nlohmann_json', required: true)
libpqxx_dep = dependency('libpqxx', required: true)
thread_dep = dependency('threads', required: true)
gtest_dep = dependency('gtest', required: true, main: false)

global_deps = [boost_dep, nlohmann_json_dep, libpqxx_dep, thread_dep]
test_deps = global_deps + gtest_dep

inc = include_directories('server/include')

src_files = files(
        'server/src/core/FileNode.cpp',
        'server/main.cpp',
)

test_sources = []

vaulthalla = executable('vaulthalla',
                        src_files,
                        dependencies: global_deps,
                        include_directories: inc,
                        install : true)

test('test_vaulthalla', executable('test_vaulthalla',
                        test_sources,
                        dependencies: test_deps,
                        include_directories: inc,
                        install : true))
