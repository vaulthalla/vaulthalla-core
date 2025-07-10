from conan import ConanFile
from conan.tools.meson import Meson

BUILD_DIR = "build"

class VaulthallaConan(ConanFile):
    name = "vaulthalla"
    version = "0.5.0"
    url = 'https://vaulthalla.io'
    description = ("A next-gen self-hosted file platform forged for the modern age. "
                   "Blazing-fast C++ backend. Beautiful Next.js frontend. Norse-tier performance.")
    settings = "os", "compiler", "arch", "build_type"
    generators = "PkgConfigDeps", "MesonToolchain"
    exports_sources = "meson.build", "src/*", "main.cpp", "tests/*", "meson/*"

    def requirements(self):
        self.requires("boost/1.88.0")
        self.requires("nlohmann_json/3.12.0")
        self.requires("libpqxx/7.10.1")
        self.requires("gtest/1.14.0")
        self.requires("libsodium/1.0.20")
        self.requires("jwt-cpp/0.7.1")
        self.requires("libcurl/8.12.1")
        self.requires("libfuse/3.16.2")
        self.requires("yaml-cpp/0.8.0")
        self.requires("libmagic/5.45")
        self.requires("stb/cci.20240531")
        self.requires("libjpeg-turbo/3.1.1", override=True)
        self.requires("pdfium/95.0.4629", options={"with_libjpeg": "libjpeg-turbo"})
        self.requires("pugixml/1.15")

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
