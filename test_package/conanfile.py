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
            examples = os.path.join(self.recipe_folder, '..', 'examples')
            rbk34 = os.path.join(examples, 'rbk34', 'raw-json.smap')
            rbk35 = os.path.join(examples, 'rbk35', 'raw-folder')
            rm34 = os.path.join(examples, 'rbk34', 'robot_model-rbk34.json')
            rm35 = os.path.join(examples, 'rbk35', 'robot_model-rbk35.json')
            self.run(
                f'{cmd} --format=rbk34 "--raw-map={rbk34}"'
                f'       --robot-model-format=rbk34 "--robot-model={rm34}"'
                f'       --format=rbk35 "--raw-map={rbk35}"'
                f'       --robot-model-format=rbk35 "--robot-model={rm35}"',
                env='conanrun',
            )
