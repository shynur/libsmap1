#include <iostream>
#include "print_pkg_info.hpp"

namespace {
    const auto _ [[maybe_unused]] = [] {
        print_pkg_info();
        return 0;
    }();
}

void print_pkg_info() {
    #ifdef NDEBUG
    std::cout << "Perfetto: Hello World Release!\n";
    #else
    std::cout << "Perfetto: Hello World Debug!\n";
    #endif

    // ARCHITECTURES
    #ifdef _M_X64
    std::cout << "  Perfetto: _M_X64 defined\n";
    #endif

    #ifdef _M_IX86
    std::cout << "  Perfetto: _M_IX86 defined\n";
    #endif

    #ifdef _M_ARM64
    std::cout << "  Perfetto: _M_ARM64 defined\n";
    #endif

    #if __i386__
    std::cout << "  Perfetto: __i386__ defined\n";
    #endif

    #if __x86_64__
    std::cout << "  Perfetto: __x86_64__ defined\n";
    #endif

    #if __aarch64__
    std::cout << "  Perfetto: __aarch64__ defined\n";
    #endif

    // Libstdc++
    #if defined _GLIBCXX_USE_CXX11_ABI
    std::cout << "  Perfetto: _GLIBCXX_USE_CXX11_ABI "<< _GLIBCXX_USE_CXX11_ABI << "\n";
    #endif

    // COMPILER VERSIONS
    #if _MSC_VER
    std::cout << "  Perfetto: _MSC_VER" << _MSC_VER<< "\n";
    #endif

    #if _MSVC_LANG
    std::cout << "  Perfetto: _MSVC_LANG" << _MSVC_LANG<< "\n";
    #endif

    #if __cplusplus
    std::cout << "  Perfetto: __cplusplus" << __cplusplus<< "\n";
    #endif

    #if __INTEL_COMPILER
    std::cout << "  Perfetto: __INTEL_COMPILER" << __INTEL_COMPILER<< "\n";
    #endif

    #if __GNUC__
    std::cout << "  Perfetto: __GNUC__" << __GNUC__<< "\n";
    #endif

    #if __GNUC_MINOR__
    std::cout << "  Perfetto: __GNUC_MINOR__" << __GNUC_MINOR__<< "\n";
    #endif

    #if __clang_major__
    std::cout << "  Perfetto: __clang_major__" << __clang_major__<< "\n";
    #endif

    #if __clang_minor__
    std::cout << "  Perfetto: __clang_minor__" << __clang_minor__<< "\n";
    #endif

    #if __apple_build_version__
    std::cout << "  Perfetto: __apple_build_version__" << __apple_build_version__<< "\n";
    #endif

    // SUBSYSTEMS

    #if __MSYS__
    std::cout << "  Perfetto: __MSYS__" << __MSYS__<< "\n";
    #endif

    #if __MINGW32__
    std::cout << "  Perfetto: __MINGW32__" << __MINGW32__<< "\n";
    #endif

    #if __MINGW64__
    std::cout << "  Perfetto: __MINGW64__" << __MINGW64__<< "\n";
    #endif

    #if __CYGWIN__
    std::cout << "  Perfetto: __CYGWIN__" << __CYGWIN__<< "\n";
    #endif
}
