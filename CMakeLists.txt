cmake_minimum_required(VERSION 3.28)

execute_process(
    COMMAND bash -c "cat ${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
    OUTPUT_VARIABLE Smap1_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

project(Smap1
    VERSION ${Smap1_VERSION}
    LANGUAGES CXX
)

#---------------- dependencies ----------------
find_package(Threads)
find_package(protobuf CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

#---------------- smap1_pb ----------------
set(Smap1_PROTO_SRC_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/proto)
set(Smap1_PROTO_GEN_DIR  ${CMAKE_CURRENT_BINARY_DIR}/proto)
file(MAKE_DIRECTORY ${Smap1_PROTO_GEN_DIR})
set(Smap1_PROTO_FILES
    ${Smap1_PROTO_SRC_DIR}/smap1/hello.proto
    ${Smap1_PROTO_SRC_DIR}/smap1/ir.proto
)

add_library(smap1_pb)

protobuf_generate(
    TARGET   smap1_pb
    LANGUAGE cpp
    IMPORT_DIRS    ${Smap1_PROTO_SRC_DIR}
    PROTOC_OUT_DIR ${Smap1_PROTO_GEN_DIR}
    PROTOS         ${Smap1_PROTO_FILES}
)

target_link_libraries(smap1_pb
    PUBLIC
        protobuf::libprotobuf
)
target_sources(smap1_pb
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${Smap1_PROTO_GEN_DIR}
        FILES
            ${Smap1_PROTO_GEN_DIR}/smap1/hello.pb.h
            ${Smap1_PROTO_GEN_DIR}/smap1/ir.pb.h
)
target_compile_features(smap1_pb
    PUBLIC
        cxx_std_26
)
set_target_properties(smap1_pb
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

#---------------- libsmap1 ----------------
include(CheckIPOSupported)
check_ipo_supported(RESULT Smap1_IPO_supported OUTPUT output)

add_library(smap1)
add_library(Smap1::smap1 ALIAS smap1)
target_sources(smap1
    PRIVATE
        src/codec.cpp
        src/geometry.cpp
        src/hello_json.cpp
        src/hello_protobuf.cpp
        src/map.cpp
        src/validate.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include/
        FILES
            include/smap1.hpp
            include/smap1/codec.hpp
            include/smap1/error.hpp
            include/smap1/geometry.hpp
            include/smap1/hello.hpp
            include/smap1/map.hpp
            include/smap1/validate.hpp
)
target_link_libraries(smap1
    PRIVATE
        Threads::Threads
    PUBLIC
        smap1_pb
        nlohmann_json::nlohmann_json
)
target_compile_features(smap1
    PUBLIC
        cxx_std_26
)
if(Smap1_IPO_supported)
    set_target_properties(smap1
        PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION TRUE
    )
endif()
set_target_properties(smap1
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
)

#--------------- install -------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
    TARGETS
        smap1
        smap1_pb
    EXPORT
        Smap1Targets
    FILE_SET
        HEADERS
)

install(
       EXPORT  Smap1Targets
  DESTINATION  ${CMAKE_INSTALL_LIBDIR}/cmake/Smap1
    NAMESPACE  Smap1::
)

install(
    FILES
        cmake/Smap1Config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Smap1
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/Smap1ConfigVersion.cmake
    COMPATIBILITY SameMajorVersion
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/Smap1ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Smap1
)
