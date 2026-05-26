#include <smap1.hpp>
#include "print_pkg_info.hpp"
#include "demo.hpp"

#include <cstdlib>

int main() {
    print_pkg_info();
    smap1::hello_json();
    smap1::hello_protobuf();
    demo_wrap();
    demo_codec(std::getenv("SMAP1_EXAMPLES"));
}
