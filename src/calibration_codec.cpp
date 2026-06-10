#include <smap1/codec.hpp>

#include "log_internal.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace smap1::codec {

namespace {

// ---- File / JSON helpers ----
//
// robot_model_codec.cpp keeps the analogous helpers in its own anonymous
// namespace; same trade-off here — duplicating two small functions beats a
// shared internal header.

std::expected<std::string, Error> read_file(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) {
        return std::unexpected{Error{Error::Code::FileNotFound, "no such file: " + p.string()}};
    }
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        return std::unexpected{Error{Error::Code::IoError, "cannot open: " + p.string()}};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    if (in.bad()) {
        return std::unexpected{Error{Error::Code::IoError, "read failed: " + p.string()}};
    }
    return ss.str();
}

std::expected<nlohmann::json, Error> parse_json(std::string_view s, std::string_view label) {
    try {
        return nlohmann::json::parse(s);
    } catch (const nlohmann::json::exception& e) {
        return std::unexpected{Error{Error::Code::ParseFailed,
            "JSON parse failed (" + std::string{label} + "): " + e.what()}};
    }
}

std::string to_lower(std::string_view s) {
    std::string out{s};
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

// 按 (device_type, device_name) 取设备条目, 不存在则按首次出现顺序追加.
proto::CalibrationDevice& device_entry(proto::Calibration& c,
                                       std::string_view type_lower,
                                       std::string_view name) {
    for (auto& d : *c.mutable_devices()) {
        if (d.device_type() == type_lower && d.device_name() == name) {
            return d;
        }
    }
    auto* d = c.add_devices();
    d->set_device_type(std::string{type_lower});
    d->set_device_name(std::string{name});
    return *d;
}

// JSON 标量 -> CalibrationParam 的 oneof.  非标量返回 false (调用方跳过).
bool set_param_value(proto::CalibrationParam& p, const nlohmann::json& v) {
    if (v.is_number()) {
        p.set_number_value(v.get<double>());
        return true;
    }
    if (v.is_string()) {
        p.set_string_value(v.get<std::string>());
        return true;
    }
    if (v.is_boolean()) {
        p.set_bool_value(v.get<bool>());
        return true;
    }
    return false;
}

// ---- rbk34 ----
//
// 顶层 {"deviceTypes":[{name, devices:[{name, deviceParams:[...]}]}]}.
// deviceParam 是 arrayParam (一层标量) / comboParam (childParams 再一层
// 标量) / 直接标量叶子, 与 robot model 文件的参数结构一致.  展开成
// "basic.x"、"func.walk.wheelRadius" 形式的点分隔 key.

// 标量叶子 {key, type, doubleValue|stringValue|boolValue|...} -> param.
void rbk34_add_leaf(proto::CalibrationDevice& dev, std::string key_prefix, const nlohmann::json& leaf) {
    auto k = leaf.find("key");
    if (k == leaf.end() || !k->is_string())
        return;
    for (const char* field : {"doubleValue", "floatValue", "stringValue", "boolValue",
                              "int32Value", "uint32Value", "int64Value", "uint64Value"}) {
        if (auto v = leaf.find(field); v != leaf.end()) {
            proto::CalibrationParam p;
            // proto3 canonical JSON 把 int64/uint64 编码成带引号的字符串;
            // 数字字段名遇到字符串值时按数字解析, 避免类型降级.
            const std::string_view fv{field};
            if (v->is_string() && (fv.starts_with("int") || fv.starts_with("uint"))) {
                try {
                    p.set_number_value(std::stod(v->get<std::string>()));
                } catch (const std::exception&) {
                    continue;
                }
            } else if (!set_param_value(p, *v)) {
                continue;
            }
            p.set_key(std::move(key_prefix) + k->get<std::string>());
            *dev.add_params() = std::move(p);
            return;
        }
    }
}

void rbk34_add_device_param(proto::CalibrationDevice& dev, const nlohmann::json& dp) {
    auto k = dp.find("key");
    if (k == dp.end() || !k->is_string())
        return;
    const std::string base = k->get<std::string>();

    if (auto ap = dp.find("arrayParam"); ap != dp.end() && ap->is_object()) {
        if (auto params = ap->find("params"); params != ap->end() && params->is_array()) {
            for (const auto& leaf : *params) {
                rbk34_add_leaf(dev, base + ".", leaf);
            }
        }
        return;
    }
    if (auto cp = dp.find("comboParam"); cp != dp.end() && cp->is_object()) {
        if (auto kids = cp->find("childParams"); kids != cp->end() && kids->is_array()) {
            for (const auto& child : *kids) {
                auto ck = child.find("key");
                if (ck == child.end() || !ck->is_string())
                    continue;
                if (auto params = child.find("params"); params != child.end() && params->is_array()) {
                    for (const auto& leaf : *params) {
                        rbk34_add_leaf(dev, base + "." + ck->get<std::string>() + ".", leaf);
                    }
                }
            }
        }
        return;
    }
    // 直接标量叶子 (无嵌套前缀).
    rbk34_add_leaf(dev, "", dp);
}

std::expected<proto::Calibration, Error> parse_rbk34(const nlohmann::json& doc) {
    auto types = doc.find("deviceTypes");
    if (types == doc.end() || !types->is_array()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk34 calibration has no 'deviceTypes' array"}};
    }
    proto::Calibration c;
    for (const auto& dt : *types) {
        auto tn = dt.find("name");
        if (tn == dt.end() || !tn->is_string())
            continue;
        const std::string type_lower = to_lower(tn->get<std::string>());
        auto devices = dt.find("devices");
        if (devices == dt.end() || !devices->is_array())
            continue;
        for (const auto& device : *devices) {
            auto dn = device.find("name");
            if (dn == device.end() || !dn->is_string())
                continue;
            auto& dev = device_entry(c, type_lower, dn->get<std::string>());
            if (auto dps = device.find("deviceParams"); dps != device.end() && dps->is_array()) {
                for (const auto& dp : *dps) {
                    rbk34_add_device_param(dev, dp);
                }
            }
        }
    }
    if (c.devices_size() == 0) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk34 calibration 'deviceTypes' yields no devices"}};
    }
    return c;
}

// ---- rbk35 ----
//
// 顶层 {"model": {"<Type>.<Name>.<param.path>": scalar, ...}}.  特例:
// - "..": null 之类的占位键 (不足三段或值为 null) 跳过;
// - 设备段后只剩一段、段名含 "Calib" 且值为字符串的项是标定状态
//   (如 "IMU.IMU-000.IMUCalib": "Passed"), 进 statuses 而非 params.

bool rbk35_is_status_key(std::string_view rest, const nlohmann::json& v) {
    return v.is_string() && rest.find('.') == std::string_view::npos
        && rest.find("Calib") != std::string_view::npos;
}

std::expected<proto::Calibration, Error> parse_rbk35(const nlohmann::json& doc) {
    auto model = doc.find("model");
    if (model == doc.end() || !model->is_object()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk35 calibration has no 'model' object"}};
    }
    proto::Calibration c;
    std::size_t skipped = 0;
    for (const auto& [key, value] : model->items()) {
        const auto first_dot = key.find('.');
        const auto second_dot = first_dot == std::string::npos
            ? std::string::npos
            : key.find('.', first_dot + 1);
        if (second_dot == std::string::npos || value.is_null()) {
            ++skipped;
            continue;
        }
        const std::string_view type{key.data(), first_dot};
        const std::string_view name{key.data() + first_dot + 1, second_dot - first_dot - 1};
        const std::string_view rest{key.data() + second_dot + 1, key.size() - second_dot - 1};
        if (type.empty() || name.empty() || rest.empty()) {
            ++skipped;
            continue;
        }

        if (rbk35_is_status_key(rest, value)) {
            auto* s = device_entry(c, to_lower(type), name).add_statuses();
            s->set_calib_type(std::string{rest});
            s->set_status(value.get<std::string>());
            continue;
        }
        proto::CalibrationParam p;
        if (!set_param_value(p, value)) {
            ++skipped;
            continue;
        }
        p.set_key(std::string{rest});
        *device_entry(c, to_lower(type), name).add_params() = std::move(p);
    }
    if (skipped > 0) {
        SMAP1_LOG_DEBUG("codec.load_calibration rbk35: skipped {} non-scalar/placeholder entries", skipped);
    }
    if (c.devices_size() == 0) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk35 calibration 'model' yields no devices"}};
    }
    return c;
}

}  // namespace

std::expected<proto::Calibration, Error> load_calibration(std::filesystem::path path, CalibrationFormat format) {
    SMAP1_LOG_INFO("codec.load_calibration path={} format={}",
        path.string(),
        format == CalibrationFormat::Rbk34 ? "rbk34" : "rbk35");
    auto bytes = read_file(path);
    if (!bytes) {
        SMAP1_LOG_ERROR("codec.load_calibration failed: {}", bytes.error().message);
        return std::unexpected{std::move(bytes).error()};
    }
    auto doc = parse_json(*bytes,
        format == CalibrationFormat::Rbk34 ? "rbk34 calibration" : "rbk35 calibration");
    if (!doc) {
        SMAP1_LOG_ERROR("codec.load_calibration failed: {}", doc.error().message);
        return std::unexpected{std::move(doc).error()};
    }

    auto parsed = (format == CalibrationFormat::Rbk34) ? parse_rbk34(*doc) : parse_rbk35(*doc);
    if (!parsed) {
        SMAP1_LOG_ERROR("codec.load_calibration failed: {}", parsed.error().message);
        return std::unexpected{std::move(parsed).error()};
    }
    parsed->set_source_path(path.generic_string());

    std::size_t params = 0, statuses = 0;
    for (const auto& d : parsed->devices()) {
        params += static_cast<std::size_t>(d.params_size());
        statuses += static_cast<std::size_t>(d.statuses_size());
    }
    SMAP1_LOG_DEBUG("codec.load_calibration ok: devices={} params={} statuses={}",
        parsed->devices_size(), params, statuses);
    return parsed;
}

}  // namespace smap1::codec
