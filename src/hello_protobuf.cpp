#include <smap1/hello.pb.h>
#include <print>

void smap1::hello_protobuf() {
    smap1::proto::Hello hello;
    hello.set_message("Hello, world!");
    hello.set_library("protobuf");
    std::println(
        "{} [library={}]",
        hello.message(),
        hello.library()
    );
}
