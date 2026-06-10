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

smap1::codec::RobotModelFormat to_rm_format(smap1::codec::SourceFormat f) {
    switch (f) {
        case smap1::codec::SourceFormat::Rbk34: return smap1::codec::RobotModelFormat::Rbk34;
        case smap1::codec::SourceFormat::Rbk35: return smap1::codec::RobotModelFormat::Rbk35;
    }
    return smap1::codec::RobotModelFormat::Rbk34;  // unreachable
}

smap1::codec::CalibrationFormat to_calib_format(smap1::codec::SourceFormat f) {
    switch (f) {
        case smap1::codec::SourceFormat::Rbk34: return smap1::codec::CalibrationFormat::Rbk34;
        case smap1::codec::SourceFormat::Rbk35: return smap1::codec::CalibrationFormat::Rbk35;
    }
    return smap1::codec::CalibrationFormat::Rbk34;  // unreachable
}

}  // namespace

int main(int argc, char **argv) {
    // 命令行结构: 只有一个 --format 决定后续 --raw-map / --robot-model /
    // --calibration 的解析方式.  --robot-model 会接到最近一次 --raw-map 解析出的
    // 地图上 (若存在), 否则只演示 robot_model 解析本身.

    std::optional<smap1::codec::SourceFormat> current_format;
    std::optional<smap1::Map> last_map;

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
            last_map.emplace(load_then_edit(std::string{path}.c_str(), *current_format));
            continue;
        }

        if (arg.starts_with("--robot-model=")) {
            if (!current_format) {
                std::println(stderr, "--robot-model 之前必须先指定 --format=<版本>");
                return 2;
            }
            auto path = arg.substr(std::string_view{"--robot-model="}.size());
            std::println("--- 加载 robot model: {} ---", path);
            auto model = load_robot_model_or_throw(std::string{path}.c_str(), to_rm_format(*current_format));
            if (last_map) {
                std::println("--- 把 robot model 接到最近的 map ---");
                demo_robot_model_with_map(*last_map, std::move(model));
            }
            continue;
        }

        if (arg.starts_with("--calibration=")) {
            if (!current_format) {
                std::println(stderr, "--calibration 之前必须先指定 --format=<版本>");
                return 2;
            }
            auto path = arg.substr(std::string_view{"--calibration="}.size());
            std::println("--- 加载标定文件: {} ---", path);
            load_calibration_or_throw(std::string{path}.c_str(), to_calib_format(*current_format));
            continue;
        }

        std::println(stderr, "未知参数: {}", arg);
        return 2;
    }
}
