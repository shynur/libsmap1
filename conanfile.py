'''
This file is used by conan v2.
'''

import os
import conan, conan.tools.cmake, conan.tools.files, conan.tools.files.symlinks

class smap1Recipe(conan.ConanFile):
    name = 'smap1'  # `s` 表示 SLAM, `1` 表示 all in one.
    package_type = 'library'

    settings = 'os', 'compiler', 'build_type', 'arch'
    options = {'shared': [True, False], 'fPIC': [True, False]}
    default_options = {'shared': True, 'fPIC': True}

    exports_sources = 'VERSION', 'CMakeLists.txt', 'cmake/*', 'src/*', 'include/*', 'proto/*'

    def set_version(self):
        self.version = conan.tools.files.load(self, 'VERSION').strip()
        self.version = self.version.lstrip('v')

    def config_options(self):
        if self.settings.os == 'Windows':
            self.options.rm_safe('fPIC')

    def configure(self):
        if self.options.shared:
            self.options.rm_safe('fPIC')

    def layout(self):
        conan.tools.cmake.cmake_layout(self)

    def requirements(self):
        self.requires('protobuf/[~6]', transitive_headers=True, transitive_libs=True)
        self.requires('nlohmann_json/[~3]', transitive_headers=True)
        self.requires('spdlog/[~1]')
        self.requires('sqlite3/[~3]', transitive_headers=True, transitive_libs=True)

    def generate(self):
        deps = conan.tools.cmake.CMakeDeps(self)
        deps.generate()
        tc = conan.tools.cmake.CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = conan.tools.cmake.CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = conan.tools.cmake.CMake(self)
        cmake.install()
        conan.tools.files.symlinks.absolute_to_relative_symlinks(
            self, self.package_folder
        )

    def package_info(self):
        self.cpp_info.set_property('cmake_target_name', 'Smap1::smap1')
        # smap1 depends on smap1_pb; list smap1 first so the linker resolves
        # symbols in the order: consumer -> smap1 -> smap1_pb -> protobuf.
        self.cpp_info.libs = ['smap1', 'smap1_pb']
