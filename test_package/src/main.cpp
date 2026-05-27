#include "demo.hpp"
#include "print_pkg_info.hpp"

#include <smap1.hpp>

#include <optional>
#include <print>
#include <string_view>

namespace {

std::optional<smap1::codec::SourceFormat> parse_format(std::string_view name) {
    if (name == "rbk34")
        return smap1::codec::SourceFormat::Rbk34;
    if (name == "rbk35")
        return smap1::codec::SourceFormat::Rbk35;
    return std::nullopt;
}

}  // namespace

int main(int argc, char **argv) {
    // print_pkg_info();
    // smap1::hello_json();
    // smap1::hello_protobuf();

    std::optional<smap1::codec::SourceFormat> current_format;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};

        if (arg.starts_with("--format=")) {
            auto value = arg.substr(std::string_view{"--format="}.size());
            current_format = parse_format(value);
            if (!current_format) {
                std::println(stderr, "未知的 --format 取值: {} (支持 rbk34 / rbk35)", value);
                return 2;
            }
            continue;
        }

        if (arg.starts_with("--raw-map=")) {
            if (!current_format) {
                std::println(stderr, "--raw-map 之前必须先指定 --format=<版本>");
                return 2;
            }
            auto path = arg.substr(std::string_view{"--raw-map="}.size());
            std::println("--- 加载并编辑地图: {} ---", path);
            load_then_edit(std::string{path}.c_str(), *current_format);
            continue;
        }

        std::println(stderr, "未知参数: {}", arg);
        return 2;
    }
}
