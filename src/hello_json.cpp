#include <smap1/hello.hpp>
#include <nlohmann/json.hpp>
#include <print>

void smap1::hello_json() {
    nlohmann::json j;
    j["message"] = "Hello, world!";
    j["library"] = "nlohmann_json";
    std::println("{}", j.dump());
}
