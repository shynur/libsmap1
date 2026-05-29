#include <smap1/codec.hpp>

#include "log_internal.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <numbers>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace smap1::codec {

namespace {

constexpr double kDegToRad = std::numbers::pi / 180.0;

// ---- File / JSON helpers ----
//
// codec.cpp keeps the analogous helpers in its own anonymous namespace; we
// duplicate the small handful we need here rather than forcing a separate
// internal header for two functions.

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

proto::Point3 read_point3(const nlohmann::json& j) {
    proto::Point3 p;
    if (j.is_object()) {
        if (auto it = j.find("x"); it != j.end() && it->is_number())
            p.set_x(it->get<double>());
        if (auto it = j.find("y"); it != j.end() && it->is_number())
            p.set_y(it->get<double>());
        if (auto it = j.find("z"); it != j.end() && it->is_number())
            p.set_z(it->get<double>());
    }
    return p;
}

// ---- shared shape population ----

std::expected<void, Error> set_rectangle(proto::RobotModel& m, double width, double head, double tail, double height) {
    if (width <= 0 || head < 0 || tail < 0) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "robot rectangle requires width > 0 and head/tail >= 0"}};
    }
    auto* r = m.mutable_rectangle();
    r->set_width(width);
    r->set_head(head);
    r->set_tail(tail);
    r->set_height(height);
    return {};
}

std::expected<void, Error> set_circle(proto::RobotModel& m, double radius, double height) {
    if (radius <= 0) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "robot circle requires radius > 0"}};
    }
    auto* c = m.mutable_circle();
    c->set_radius(radius);
    c->set_height(height);
    return {};
}

// rbk34 stores polygon vertices as a JSON-encoded string of [{x,y}, ...];
// rbk35 wraps them as [{"shape":"polygon","points":[{x,y}, ...]}].
std::expected<void, Error> set_polygon_from_string(proto::RobotModel& m, std::string_view raw, double height) {
    auto doc = parse_json(raw, "robot polygon");
    if (!doc) {
        return std::unexpected{std::move(doc).error()};
    }
    auto* poly = m.mutable_polygon();
    poly->set_height(height);

    auto consume_points = [&](const nlohmann::json& arr) {
        if (!arr.is_array())
            return;
        for (const auto& v : arr) {
            *poly->add_vertices() = read_point3(v);
        }
    };

    if (doc->is_array() && !doc->empty()) {
        const auto& first = (*doc)[0];
        if (first.is_object() && first.contains("points")) {
            consume_points(first["points"]);
        } else {
            consume_points(*doc);
        }
    }
    if (poly->vertices_size() == 0) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "robot polygon has no vertices"}};
    }
    return {};
}

// ---- rbk34 ----
//
// chassis lives at deviceTypes[name=="chassis"].devices[0].deviceParams; the
// shape is a comboParam with childKey ∈ {rectangle, circle, polygon}.

const nlohmann::json* find_device_type(const nlohmann::json& doc, std::string_view name) {
    auto it = doc.find("deviceTypes");
    if (it == doc.end() || !it->is_array())
        return nullptr;
    for (const auto& dt : *it) {
        if (auto n = dt.find("name"); n != dt.end() && n->is_string() && n->get<std::string>() == name) {
            return &dt;
        }
    }
    return nullptr;
}

const nlohmann::json* find_device_param(const nlohmann::json& device, std::string_view key) {
    auto it = device.find("deviceParams");
    if (it == device.end() || !it->is_array())
        return nullptr;
    for (const auto& dp : *it) {
        if (auto k = dp.find("key"); k != dp.end() && k->is_string() && k->get<std::string>() == key) {
            return &dp;
        }
    }
    return nullptr;
}

double rbk34_param_double(const nlohmann::json& params, std::string_view key, double dflt = 0.0) {
    if (!params.is_array())
        return dflt;
    for (const auto& p : params) {
        if (auto k = p.find("key"); k != p.end() && k->is_string() && k->get<std::string>() == key) {
            if (auto v = p.find("doubleValue"); v != p.end() && v->is_number()) {
                return v->get<double>();
            }
        }
    }
    return dflt;
}

const nlohmann::json* rbk34_param_obj(const nlohmann::json& params, std::string_view key) {
    if (!params.is_array())
        return nullptr;
    for (const auto& p : params) {
        if (auto k = p.find("key"); k != p.end() && k->is_string() && k->get<std::string>() == key) {
            return &p;
        }
    }
    return nullptr;
}

std::expected<proto::RobotModel, Error> parse_rbk34(const nlohmann::json& doc) {
    proto::RobotModel m;
    if (auto it = doc.find("model"); it != doc.end() && it->is_string()) {
        m.set_model_id(it->get<std::string>());
    }

    const nlohmann::json* chassis = find_device_type(doc, "chassis");
    if (!chassis) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk34 robot model has no 'chassis' deviceType"}};
    }
    const auto& devices = (*chassis)["devices"];
    if (!devices.is_array() || devices.empty()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk34 robot model chassis has no devices"}};
    }
    const auto& device = devices[0];

    // basic.shape2chassis_{x,y,theta}
    if (const auto* basic = find_device_param(device, "basic")) {
        if (auto ap = basic->find("arrayParam"); ap != basic->end()) {
            if (auto pp = ap->find("params"); pp != ap->end()) {
                auto* s2c = m.mutable_shape_to_chassis();
                s2c->mutable_position()->set_x(rbk34_param_double(*pp, "shape2chassis_x"));
                s2c->mutable_position()->set_y(rbk34_param_double(*pp, "shape2chassis_y"));
                s2c->set_heading_rad(rbk34_param_double(*pp, "shape2chassis_theta") * kDegToRad);
            }
        }
    }

    // shape comboParam
    const auto* shape = find_device_param(device, "shape");
    if (!shape) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk34 robot model chassis has no 'shape' parameter"}};
    }
    auto cp = shape->find("comboParam");
    if (cp == shape->end()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk34 chassis.shape is not a comboParam"}};
    }
    const std::string selected = cp->value("childKey", std::string{});
    auto kids = cp->find("childParams");
    if (kids == cp->end() || !kids->is_array()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk34 chassis.shape has no childParams"}};
    }
    const nlohmann::json* match = nullptr;
    for (const auto& child : *kids) {
        if (child.value("key", std::string{}) == selected) {
            match = &child;
            break;
        }
    }
    if (!match) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk34 chassis.shape selected child '" + selected + "' missing"}};
    }
    const auto& params = (*match)["params"];
    if (selected == "rectangle") {
        if (auto r = set_rectangle(m,
                rbk34_param_double(params, "width"),
                rbk34_param_double(params, "head"),
                rbk34_param_double(params, "tail"),
                rbk34_param_double(params, "height"));
            !r) return std::unexpected{r.error()};
    } else if (selected == "circle") {
        if (auto r = set_circle(m,
                rbk34_param_double(params, "radius"),
                rbk34_param_double(params, "height"));
            !r) return std::unexpected{r.error()};
    } else if (selected == "polygon") {
        const auto* p = rbk34_param_obj(params, "polygon");
        std::string raw = p ? p->value("stringValue", std::string{}) : std::string{};
        if (auto r = set_polygon_from_string(m, raw,
                rbk34_param_double(params, "height", 0.0));
            !r) return std::unexpected{r.error()};
    } else {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk34 chassis.shape unknown childKey '" + selected + "'"}};
    }
    return m;
}

// ---- rbk35 ----
//
// shape lives at groups[key=="Model"].children[0].groups[key=="shape"]; the
// selected child is named by `value`, and each child carries its own scalar
// children.

const nlohmann::json* rbk35_find_group(const nlohmann::json& groups, std::string_view key) {
    if (!groups.is_array())
        return nullptr;
    for (const auto& g : groups) {
        if (auto k = g.find("key"); k != g.end() && k->is_string() && k->get<std::string>() == key) {
            return &g;
        }
    }
    return nullptr;
}

double rbk35_child_double(const nlohmann::json& group, std::string_view key, double dflt = 0.0) {
    auto it = group.find("children");
    if (it == group.end() || !it->is_array())
        return dflt;
    for (const auto& c : *it) {
        if (c.value("key", std::string{}) == key) {
            if (auto v = c.find("value"); v != c.end() && v->is_number()) {
                return v->get<double>();
            }
        }
    }
    return dflt;
}

const nlohmann::json* rbk35_child(const nlohmann::json& group, std::string_view key) {
    auto it = group.find("children");
    if (it == group.end() || !it->is_array())
        return nullptr;
    for (const auto& c : *it) {
        if (c.value("key", std::string{}) == key) {
            return &c;
        }
    }
    return nullptr;
}

std::expected<proto::RobotModel, Error> parse_rbk35(const nlohmann::json& doc) {
    proto::RobotModel m;

    // 注意: rbk35_find_group 返回内部引用, 因此必须传入长寿引用; 临时
    // doc.value(...) 返回的拷贝会立刻析构, 留下悬挂指针.
    auto groups_it = doc.find("groups");
    if (groups_it == doc.end()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk35 robot model has no 'groups' array"}};
    }
    const auto* model_top = rbk35_find_group(*groups_it, "Model");
    if (!model_top) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk35 robot model has no 'Model' group"}};
    }
    auto children_it = model_top->find("children");
    if (children_it == model_top->end() || !children_it->is_array() || children_it->empty()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk35 'Model' group has no children"}};
    }
    const auto& model_node = (*children_it)[0];
    if (auto it = model_node.find("name"); it != model_node.end() && it->is_string()) {
        m.set_model_id(it->get<std::string>());
    }

    auto inner_groups_it = model_node.find("groups");
    if (inner_groups_it == model_node.end()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk35 model node has no 'groups'"}};
    }
    const auto* shape = rbk35_find_group(*inner_groups_it, "shape");
    if (!shape) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk35 model has no 'shape' group"}};
    }
    const std::string selected = shape->value("value", std::string{});
    const auto* selected_node = rbk35_child(*shape, selected);
    if (!selected_node) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk35 shape selected child '" + selected + "' missing"}};
    }

    if (selected == "rectangle") {
        if (auto r = set_rectangle(m,
                rbk35_child_double(*selected_node, "width"),
                rbk35_child_double(*selected_node, "head"),
                rbk35_child_double(*selected_node, "tail"),
                rbk35_child_double(*selected_node, "height"));
            !r) return std::unexpected{r.error()};
    } else if (selected == "circle") {
        if (auto r = set_circle(m,
                rbk35_child_double(*selected_node, "radius"),
                rbk35_child_double(*selected_node, "height"));
            !r) return std::unexpected{r.error()};
    } else if (selected == "polygon") {
        std::string raw;
        if (const auto* p = rbk35_child(*selected_node, "polygon")) {
            raw = p->value("value", std::string{});
        }
        if (auto r = set_polygon_from_string(m, raw,
                rbk35_child_double(*selected_node, "height", 0.0));
            !r) return std::unexpected{r.error()};
    } else {
        return std::unexpected{Error{Error::Code::InvalidArgument,
            "rbk35 shape unknown selected '" + selected + "'"}};
    }
    // rbk35 doesn't expose shape2chassis offsets in its model file; the
    // default-zero shape_to_chassis is left as-is.
    return m;
}

}  // namespace

std::expected<proto::RobotModel, Error> load_robot_model(std::filesystem::path path, RobotModelFormat format) {
    SMAP1_LOG_INFO("codec.load_robot_model path={} format={}",
        path.string(),
        format == RobotModelFormat::Rbk34 ? "rbk34" : "rbk35");
    auto bytes = read_file(path);
    if (!bytes) {
        SMAP1_LOG_ERROR("codec.load_robot_model failed: {}", bytes.error().message);
        return std::unexpected{std::move(bytes).error()};
    }
    auto doc = parse_json(*bytes,
        format == RobotModelFormat::Rbk34 ? "rbk34 robot_model" : "rbk35 robot_model");
    if (!doc) {
        SMAP1_LOG_ERROR("codec.load_robot_model failed: {}", doc.error().message);
        return std::unexpected{std::move(doc).error()};
    }

    auto parsed = (format == RobotModelFormat::Rbk34) ? parse_rbk34(*doc) : parse_rbk35(*doc);
    if (!parsed) {
        SMAP1_LOG_ERROR("codec.load_robot_model failed: {}", parsed.error().message);
        return std::unexpected{std::move(parsed).error()};
    }
    parsed->set_source_path(path.generic_string());
    SMAP1_LOG_DEBUG("codec.load_robot_model ok: model_id='{}' shape_case={}",
        parsed->model_id(),
        static_cast<int>(parsed->shape_case()));
    return parsed;
}

}  // namespace smap1::codec
