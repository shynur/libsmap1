import os
import conan, conan.tools.cmake, conan.tools.build


class PerfettoTestConan(conan.ConanFile):
    settings = 'os', 'compiler', 'build_type', 'arch'
    generators = 'CMakeDeps', 'CMakeToolchain', 'VirtualBuildEnv', 'VirtualRunEnv'
    apply_env = False
    test_type = 'explicit'

    def requirements(self):
        self.requires(self.tested_reference_str)

    def build(self):
        cmake = conan.tools.cmake.CMake(self)
        cmake.configure()
        cmake.build()

    def layout(self):
        conan.tools.cmake.cmake_layout(self)

    def test(self):
        if not conan.tools.build.cross_building(self):
            cmd = os.path.join(self.cpp.build.bindirs[0], 'example')
            self.run(cmd, env='conanrun')
