from conan import ConanFile
from conan.tools.meson import Meson

BUILD_DIR = "build"

class VaulthallaConan(ConanFile):
    name = "vaulthalla"
    version = "0.0.1"
    url = 'https://vaulthalla.io'
    description = ("A next-gen self-hosted file platform forged for the modern age. "
                   "Blazing-fast C++ backend. Beautiful Next.js frontend. Norse-tier performance.")
    settings = "os", "compiler", "arch", "build_type"
    requires = [
        "boost/1.88.0",
        "nlohmann_json/3.12.0",
        "libpqxx/7.10.1",
        "gtest/1.14.0",
        "libsodium/1.0.20",
        "jwt-cpp/0.7.1",
        "libenvpp/1.5.0"
    ]
    generators = "PkgConfigDeps", "MesonToolchain"
    exports_sources = "meson.build", "src/*", "main.cpp", "tests/*", "meson/*"

    def layout(self):
        self.folders.source = '.'
        self.folders.build = f'{BUILD_DIR}'
        self.folders.generators = f'{BUILD_DIR}/generators'
        self.folders.package = f'{BUILD_DIR}/package'

    def build(self):
        meson = Meson(self)
        meson.configure()
        meson.build()

    def test(self):
        meson = Meson(self)
        meson.test()

    def package(self):
        meson = Meson(self)
        meson.install()
