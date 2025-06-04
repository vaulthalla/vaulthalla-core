from conan import ConanFile
from conan.tools.meson import Meson

BUILD_DIR = "server/build"

class VaulthallaConan(ConanFile):
    name = "vaulthalla"
    version = "0.0.1"
    settings = "os", "compiler", "arch", "build_type"
    description = ("A next-gen self-hosted file platform forged for the modern age. "
                   "Blazing-fast C++ backend. Beautiful Next.js frontend. Norse-tier performance.")
    requires = [
        "boost/1.85.0",
        "nlohmann_json/3.11.3",
        "libpqxx/7.9.1",
        "gtest/1.14.0",
    ]
    generators = "PkgConfigDeps", "MesonToolchain"
    exports_sources = "meson.build", "src/*", "main.cpp", "tests/*", "meson/*"

    def layout(self):
        self.folders.source = 'server'
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
