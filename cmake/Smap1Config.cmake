include(${CMAKE_CURRENT_LIST_DIR}/Smap1Targets.cmake)

include(CMakeFindDependencyMacro)
find_dependency(Threads)
find_dependency(protobuf CONFIG)
find_dependency(nlohmann_json CONFIG)
