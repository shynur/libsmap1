cmake_minimum_required(VERSION 3.28)
project(Example LANGUAGES CXX)

if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
    )
    add_link_options(-pthread)
endif()

#------------------------------------------
add_executable(example)
target_sources(example
    PRIVATE
        src/main.cpp
        src/print_pkg_info.cpp
    PRIVATE
        FILE_SET  HEADERS
        BASE_DIRS include
)
target_link_libraries(example
    PRIVATE
        Smap1::smap1
)

#----------------------------------------
find_package(Smap1 CONFIG REQUIRED)
