#include <smap1/codec.hpp>

#include "log_internal.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace smap1::codec {

namespace {

// ---- File / JSON helpers ----
//
// Duplicated from calibration_codec.cpp / robot_model_codec.cpp — same
// trade-off: two small functions per codec beats a shared internal header.

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

// ---- Safe JSON field extraction helpers ----

std::optional<int> json_int(const nlohmann::json* parent, std::string_view key) {
    if (!parent) return std::nullopt;
    auto it = parent->find(key);
    if (it == parent->end() || !it->is_number_integer())
        return std::nullopt;
    return it->get<int>();
}

std::optional<double> json_double(const nlohmann::json* parent, std::string_view key) {
    if (!parent) return std::nullopt;
    auto it = parent->find(key);
    if (it == parent->end() || !it->is_number())
        return std::nullopt;
    return it->get<double>();
}

std::optional<bool> json_bool(const nlohmann::json* parent, std::string_view key) {
    if (!parent) return std::nullopt;
    auto it = parent->find(key);
    if (it == parent->end() || !it->is_boolean())
        return std::nullopt;
    return it->get<bool>();
}

std::string json_str(const nlohmann::json* parent, std::string_view key, std::string_view dflt = "") {
    if (!parent) return std::string{dflt};
    auto it = parent->find(key);
    if (it == parent->end() || !it->is_string())
        return std::string{dflt};
    return it->get<std::string>();
}

const nlohmann::json* json_obj(const nlohmann::json* parent, std::string_view key) {
    if (!parent) return nullptr;
    auto it = parent->find(key);
    if (it == parent->end() || !it->is_object())
        return nullptr;
    return &*it;
}

const nlohmann::json* json_arr(const nlohmann::json* parent, std::string_view key) {
    if (!parent) return nullptr;
    auto it = parent->find(key);
    if (it == parent->end() || !it->is_array())
        return nullptr;
    return &*it;
}

// ---- Container helpers ----
//
// Both rbk34 (Message_Container) and rbk35 (msgContainer) have the same
// shape: {container_name/containerName, goods_id/goodsId, has_goods/hasGoods,
// desc}.  We accept both snake_case and camelCase for each field.

void add_container(proto::VehicleContainer& vc, const nlohmann::json& c) {
    auto* container = vc.add_containers();
    // name: try "containerName" then "container_name"
    container->set_name(json_str(&c, "containerName",
        json_str(&c, "container_name")));
    // goods_id: try "goodsId" then "goods_id"
    container->set_goods_id(json_str(&c, "goodsId",
        json_str(&c, "goods_id")));
    // has_goods: try "hasGoods" then "has_goods"
    if (auto hg = json_bool(&c, "hasGoods")) {
        container->set_has_goods(*hg);
    } else if (auto hg = json_bool(&c, "has_goods")) {
        container->set_has_goods(*hg);
    }
    // desc / description
    container->set_description(json_str(&c, "desc",
        json_str(&c, "description")));
}

// ---- Vehicle state mapping ----
//
// rbk34 VehicleState (route.v1 / route.v2) and our proto enum share the same
// numeric values (0=UNKNOWN … 5=CHARGING), so no translation needed beyond
// a safety clamp.

proto::Vehicle_State map_vehicle_state(int raw) {
    switch (raw) {
        case 0: return proto::Vehicle_State_VEHICLE_STATE_UNKNOWN;
        case 1: return proto::Vehicle_State_VEHICLE_STATE_UNAVAILABLE;
        case 2: return proto::Vehicle_State_VEHICLE_STATE_ERROR;
        case 3: return proto::Vehicle_State_VEHICLE_STATE_IDLE;
        case 4: return proto::Vehicle_State_VEHICLE_STATE_EXECUTING;
        case 5: return proto::Vehicle_State_VEHICLE_STATE_CHARGING;
        default:
            SMAP1_LOG_DEBUG("codec.load_vehicle_container: unknown VehicleState={}, using UNKNOWN", raw);
            return proto::Vehicle_State_VEHICLE_STATE_UNKNOWN;
    }
}

proto::Vehicle_ExecutingState map_exec_state(int raw) {
    switch (raw) {
        case 0: return proto::Vehicle_ExecutingState_EXEC_STATE_NONE;
        case 1: return proto::Vehicle_ExecutingState_EXEC_STATE_MOVING;
        case 2: return proto::Vehicle_ExecutingState_EXEC_STATE_OPERATING;
        default:
            return proto::Vehicle_ExecutingState_EXEC_STATE_NONE;
    }
}

// ---- ── rbk34 ── ----
//
// Parses a JSON-serialized Message_RBK_Response (route.v1) or
// Message_Response_Status (route.v2).  Both share the same core fields:
//   state (int), position (string), energyLevel (uint),
//   loadHandlingDevices (array of {label, full}).
//
// v1 additionally has isOnload, isCmdConfirmation, isPaused.
// v2 additionally has exeState (int), errorInfos (array of {timestamp, level,
// message, ...}) — the v1 error format uses {timestamp, count, level, message},
// both are flattened to the VehicleError IR.

void add_load_device(proto::Vehicle& v, const nlohmann::json& device_json) {
    auto* d = v.add_load_devices();
    d->set_label(json_str(&device_json, "label"));
    if (auto full = json_bool(&device_json, "full")) {
        d->set_full(*full);
    }
}

void add_error(proto::Vehicle& v, const nlohmann::json& err_json) {
    auto* e = v.add_errors();
    e->set_timestamp(json_str(&err_json, "timestamp"));
    // v2 uses "level"; v1 also uses "level"
    e->set_level(json_str(&err_json, "level"));
    // v2 uses "message"; v1 also uses "message"
    e->set_message(json_str(&err_json, "message"));
}

std::expected<proto::VehicleContainer, Error> parse_rbk34(const nlohmann::json& doc) {
    proto::VehicleContainer vc;

    // --- primary vehicle ---
    auto* vehicle = vc.add_vehicles();

    // state
    if (auto s = json_int(&doc, "state")) {
        vehicle->set_state(map_vehicle_state(*s));
    }

    // position
    vehicle->set_position(json_str(&doc, "position"));

    // energyLevel
    if (auto e = json_int(&doc, "energyLevel")) {
        vehicle->set_energy_level(static_cast<uint32_t>(*e));
    }

    // isOnload (v1)  — also try is_loaded (alternative naming)
    if (auto l = json_bool(&doc, "isOnload")) {
        vehicle->set_is_loaded(*l);
    } else if (auto l = json_bool(&doc, "is_loaded")) {
        vehicle->set_is_loaded(*l);
    }

    // exeState (v2 only)
    if (auto es = json_int(&doc, "exeState")) {
        vehicle->set_executing_state(map_exec_state(*es));
    }

    // loadHandlingDevices → both vehicle.load_devices and global containers
    if (const auto* devices = json_arr(&doc, "loadHandlingDevices")) {
        for (const auto& d : *devices) {
            if (!d.is_object()) continue;
            add_load_device(*vehicle, d);

            // Also emit as a global container (label → name, full → has_goods)
            auto* container = vc.add_containers();
            container->set_name(json_str(&d, "label"));
            if (auto full = json_bool(&d, "full")) {
                container->set_has_goods(*full);
            }
        }
    }

    // errorInfos (v2) — v1 error format also uses "errorInfos" (with "count"
    // instead of "level"/"message" structure, but both have "level" and
    // "message" when present; we just skip entries that lack them).
    if (const auto* errors = json_arr(&doc, "errorInfos")) {
        for (const auto& e : *errors) {
            if (!e.is_object()) continue;
            add_error(*vehicle, e);
        }
    }

    // --- containers from optional inline containers array ---
    // Some rbk34 logs embed a Message_MoveStatus alongside the response;
    // we opportunistically check field 44 ("containers").
    if (const auto* containers = json_arr(&doc, "containers")) {
        for (const auto& c : *containers) {
            if (!c.is_object()) continue;
            add_container(vc, c);
        }
    }

    // If we got at least a vehicle with a state set (or any containers),
    // consider the parse successful.  Empty is allowed (caller can check).
    if (vc.vehicles_size() == 0 && vc.containers_size() == 0) {
        SMAP1_LOG_DEBUG("codec.load_vehicle_container rbk34: parsed top-level JSON, but found no vehicles or containers");
    }

    return vc;
}

// ---- ── rbk35 ── ----
//
// Handles two sub-formats:
//
// 1. msgState (comprehensive robot push state) — nested JSON with
//    top-level keys: robot, battery, navigation, chassis, etc.
//    - Robot info: doc["robot"]["vehicleId"] / doc["robot"]["id"]
//    - Battery:    doc["battery"]["level"]
//    - Errors:     doc["robot"]["errors"] (map<string, msgError>)
//    - Containers: doc["navigation"]["moveStatusInfo"] is a JSON string
//      containing a serialized msgMoveStatus; from that, extract
//      ["containers"].
//
// 2. msgMoveStatus (standalone) — flat JSON with top-level key
//    "containers" (repeated msgContainer).
//
// The parser probes doc["robot"] first: if it exists and is an object,
// treat as msgState; otherwise fall back to the flat structure.

void add_rbk35_containers_from_movestatus_json(proto::VehicleContainer& vc,
                                               const std::string& raw_json) {
    auto parsed = parse_json(raw_json, "rbk35 moveStatusInfo");
    if (!parsed) {
        SMAP1_LOG_DEBUG("codec.load_vehicle_container rbk35: moveStatusInfo is not valid JSON, skipping");
        return;
    }
    auto containers = parsed->find("containers");
    if (containers == parsed->end() || !containers->is_array()) {
        return;
    }
    for (const auto& c : *containers) {
        if (!c.is_object()) continue;
        add_container(vc, c);
    }
}

void add_rbk35_errors_from_robot(proto::Vehicle& v, const nlohmann::json& errors_map) {
    if (!errors_map.is_object()) return;
    for (const auto& [key, val] : errors_map.items()) {
        if (!val.is_object()) continue;
        // msgError has: deviceError/configError/mapError/... (oneof),
        // desc (string), manual (bool), timeStamp (uint64).
        // We only extract the common fields for the IR.
        auto* e = v.add_errors();
        // Use the map key as a contextual hint in "level"
        e->set_level(key);
        e->set_message(val.value("desc", std::string{}));
        // timeStamp comes as a string in protobuf JSON (uint64 → decimal string)
        if (auto ts = val.find("timeStamp"); ts != val.end()) {
            if (ts->is_string())
                e->set_timestamp(ts->get<std::string>());
            else if (ts->is_number())
                e->set_timestamp(std::to_string(ts->get<uint64_t>()));
        }
    }
}

std::expected<proto::VehicleContainer, Error> parse_rbk35(const nlohmann::json& doc) {
    proto::VehicleContainer vc;

    // Probe: is this a msgState (has "robot") or a flat format?
    const auto* robot_section = json_obj(&doc, "robot");

    if (robot_section) {
        // ── msgState format ──
        auto* vehicle = vc.add_vehicles();

        // Robot identity
        vehicle->set_id(json_str(robot_section, "id",
            json_str(robot_section, "vehicleId")));
        vehicle->set_name(json_str(robot_section, "vehicleId",
            json_str(robot_section, "name")));

        // Errors from robot.errors (map<string, msgError>)
        if (const auto* errors = json_obj(robot_section, "errors")) {
            add_rbk35_errors_from_robot(*vehicle, *errors);
        }

        // Battery level — doc["battery"]["level"]
        if (const auto* battery = json_obj(&doc, "battery")) {
            if (auto lvl = json_double(battery, "level")) {
                vehicle->set_energy_level(static_cast<uint32_t>(std::clamp(*lvl, 0.0, 100.0)));
            }
        }

        // Loading state — try doc["chassis"] for load info;
        // no standard "isLoaded" in msgPushChassisInfo, but some deployments
        // extend it.  Also try a top-level "isLoaded" or "loaded".
        if (auto loaded = json_bool(&doc, "isLoaded")) {
            vehicle->set_is_loaded(*loaded);
        } else if (auto loaded = json_bool(&doc, "loaded")) {
            vehicle->set_is_loaded(*loaded);
        }

        // Navigation status
        if (const auto* nav = json_obj(&doc, "navigation")) {
            // taskStatus → state mapping
            if (auto ts = json_int(nav, "taskStatus")) {
                // msgMoveStatus.status enum: 0=statusNone, 1=waiting,
                // 2=running, 3=suspended, 4=completed, 5=failed
                // Map to VehicleState: running/executing → EXECUTING,
                // waiting/suspended → IDLE, completed → IDLE, failed → ERROR
                switch (*ts) {
                    case 4:  // completed
                    case 3:  // suspended
                    case 1:  // waiting
                        vehicle->set_state(proto::Vehicle_State_VEHICLE_STATE_IDLE);
                        break;
                    case 2:  // running
                        vehicle->set_state(proto::Vehicle_State_VEHICLE_STATE_EXECUTING);
                        break;
                    case 5:  // failed
                        vehicle->set_state(proto::Vehicle_State_VEHICLE_STATE_ERROR);
                        break;
                    default:
                        break;
                }
            }

            // moveStatusInfo → containers
            auto move_info = json_str(nav, "moveStatusInfo");
            if (!move_info.empty()) {
                add_rbk35_containers_from_movestatus_json(vc, move_info);
            }
        }

    } else {
        // ── Flat format: could be msgMoveStatus or a simple msgState-standalone ──
        // Check for top-level "containers" (msgMoveStatus style)
        if (const auto* containers = json_arr(&doc, "containers")) {
            for (const auto& c : *containers) {
                if (!c.is_object()) continue;
                add_container(vc, c);
            }
        }

        // Also try flat vehicle fields (like rbk34 but rbk35 naming)
        if (doc.contains("vehicleId") || doc.contains("id")) {
            auto* vehicle = vc.add_vehicles();
            vehicle->set_id(json_str(&doc, "id", json_str(&doc, "vehicleId")));
            vehicle->set_name(json_str(&doc, "vehicleId"));
            if (auto s = json_int(&doc, "state")) {
                vehicle->set_state(map_vehicle_state(*s));
            }
            vehicle->set_position(json_str(&doc, "position"));
            if (auto e = json_int(&doc, "energyLevel")) {
                vehicle->set_energy_level(static_cast<uint32_t>(*e));
            }
            if (auto l = json_bool(&doc, "isLoaded")) {
                vehicle->set_is_loaded(*l);
            }
        }
    }

    if (vc.vehicles_size() == 0 && vc.containers_size() == 0) {
        SMAP1_LOG_DEBUG("codec.load_vehicle_container rbk35: parsed JSON but found no vehicles or containers");
    }

    return vc;
}

}  // anonymous namespace

std::expected<proto::VehicleContainer, Error> load_vehicle_container(
    std::filesystem::path path, VehicleContainerFormat format) {

    SMAP1_LOG_INFO("codec.load_vehicle_container path={} format={}",
        path.string(),
        format == VehicleContainerFormat::Rbk34 ? "rbk34" : "rbk35");

    auto bytes = read_file(path);
    if (!bytes) {
        SMAP1_LOG_ERROR("codec.load_vehicle_container failed: {}", bytes.error().message);
        return std::unexpected{std::move(bytes).error()};
    }

    auto doc = parse_json(*bytes,
        format == VehicleContainerFormat::Rbk34 ? "rbk34 vehicle/container" : "rbk35 vehicle/container");
    if (!doc) {
        SMAP1_LOG_ERROR("codec.load_vehicle_container failed: {}", doc.error().message);
        return std::unexpected{std::move(doc).error()};
    }

    auto parsed = (format == VehicleContainerFormat::Rbk34) ? parse_rbk34(*doc) : parse_rbk35(*doc);
    if (!parsed) {
        SMAP1_LOG_ERROR("codec.load_vehicle_container failed: {}", parsed.error().message);
        return std::unexpected{std::move(parsed).error()};
    }

    parsed->set_source_path(path.generic_string());

    std::size_t n_vehicles = static_cast<std::size_t>(parsed->vehicles_size());
    std::size_t n_containers = static_cast<std::size_t>(parsed->containers_size());
    SMAP1_LOG_DEBUG("codec.load_vehicle_container ok: vehicles={} containers={}",
        n_vehicles, n_containers);

    return parsed;
}

}  // namespace smap1::codec
