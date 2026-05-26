#include <smap1/codec.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <numbers>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace smap1::codec {

namespace {

namespace fs = std::filesystem;
using nlohmann::json;
using StationLookup = std::unordered_map<std::string, const proto::Station*>;

// ---- File I/O ----

std::expected<std::string, Error> read_file(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(p, ec)) {
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

std::expected<void, Error> write_file(const fs::path& p, std::string_view bytes) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out) {
        return std::unexpected{Error{Error::Code::IoError, "cannot open for write: " + p.string()}};
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out.good()) {
        return std::unexpected{Error{Error::Code::IoError, "write failed: " + p.string()}};
    }
    return {};
}

std::expected<json, Error> parse_json(std::string_view s, std::string_view label) {
    try {
        return json::parse(s);
    } catch (const json::exception& e) {
        return std::unexpected{Error{Error::Code::ParseFailed,
            "JSON parse failed (" + std::string{label} + "): " + e.what()}};
    }
}

bool target_is_nonempty(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(p, ec)) return false;
    if (fs::is_regular_file(p, ec)) {
        return fs::file_size(p, ec) > 0;
    }
    if (fs::is_directory(p, ec)) {
        return fs::directory_iterator{p, ec} != fs::directory_iterator{};
    }
    return true;
}

// ---- Angle normalization ----
//
// IR stores Pose2D::heading_rad in radians (see docs/ir.md). rbk34 already
// uses radians; rbk35 stores degrees in `dir`. Both directions go through
// these helpers so any future format can declare its native unit in one place.

constexpr double kDegToRad = std::numbers::pi / 180.0;
constexpr double kRadToDeg = 180.0 / std::numbers::pi;

double dir_to_rad(double raw, SourceFormat fmt) {
    return fmt == SourceFormat::Rbk35 ? raw * kDegToRad : raw;
}

double rad_to_dir(double rad, SourceFormat fmt) {
    return fmt == SourceFormat::Rbk35 ? rad * kRadToDeg : rad;
}

// ---- Geometry primitives ----

proto::Point3 read_point3(const json& j) {
    proto::Point3 p;
    if (j.is_object()) {
        if (auto it = j.find("x"); it != j.end() && it->is_number()) p.set_x(it->get<double>());
        if (auto it = j.find("y"); it != j.end() && it->is_number()) p.set_y(it->get<double>());
        if (auto it = j.find("z"); it != j.end() && it->is_number()) p.set_z(it->get<double>());
    }
    return p;
}

json point3_to_json(const proto::Point3& p, bool include_z) {
    json j{{"x", p.x()}, {"y", p.y()}};
    if (include_z) j["z"] = p.z();
    return j;
}

// ---- Property bag ----
//
// Property entries vary substantially across formats and even within a single
// format (e.g. rbk34's typed boolValue / int32Value alongside a base64-encoded
// `value`). To keep round-tripping lossless without modeling every variant, we
// stash each entry's full JSON object in Property.value.raw_json_text and copy
// the obvious string keys (key / type / tag) into top-level Property fields
// for visibility.

void read_properties(const json& arr, google::protobuf::RepeatedPtrField<proto::Property>& out) {
    if (!arr.is_array()) return;
    for (const auto& entry : arr) {
        auto* p = out.Add();
        if (entry.is_object()) {
            if (auto it = entry.find("key"); it != entry.end() && it->is_string()) {
                p->set_key(it->get<std::string>());
            }
            if (auto it = entry.find("type"); it != entry.end() && it->is_string()) {
                p->set_type_hint(it->get<std::string>());
            }
            if (auto it = entry.find("tag"); it != entry.end() && it->is_string()) {
                p->set_tag(it->get<std::string>());
            }
        }
        p->mutable_value()->set_raw_json_text(entry.dump());
    }
}

json write_properties(const google::protobuf::RepeatedPtrField<proto::Property>& v) {
    auto out = json::array();
    for (const auto& p : v) {
        if (p.value().kind_case() == proto::PropertyValue::kRawJsonText) {
            const auto& raw = p.value().raw_json_text();
            if (!raw.empty()) {
                try {
                    out.push_back(json::parse(raw));
                    continue;
                } catch (const json::exception&) {
                    // Fall through and synthesize a best-effort entry below.
                }
            }
        }
        json e{{"key", p.key()}};
        if (!p.type_hint().empty()) e["type"] = p.type_hint();
        if (!p.tag().empty()) e["tag"] = p.tag();
        out.push_back(std::move(e));
    }
    return out;
}

// ---- Station ----

proto::Station read_station(const json& j, SourceFormat fmt) {
    proto::Station s;
    if (auto it = j.find("instanceName"); it != j.end() && it->is_string()) {
        s.set_id(it->get<std::string>());
        s.set_name(it->get<std::string>());
    }
    if (auto it = j.find("className"); it != j.end() && it->is_string()) {
        s.set_class_name(it->get<std::string>());
    }
    if (auto it = j.find("pos"); it != j.end()) {
        *s.mutable_pose()->mutable_position() = read_point3(*it);
    }
    if (auto it = j.find("dir"); it != j.end() && it->is_number()) {
        s.mutable_pose()->set_heading_rad(dir_to_rad(it->get<double>(), fmt));
    }
    if (auto it = j.find("ignoreDir"); it != j.end() && it->is_boolean()) {
        s.set_ignore_heading(it->get<bool>());
    }
    if (auto it = j.find("desc"); it != j.end() && it->is_string()) {
        const auto& d = it->get_ref<const std::string&>();
        s.set_description(d.data(), d.size());
    }
    if (auto it = j.find("property"); it != j.end()) {
        read_properties(*it, *s.mutable_properties());
    }
    return s;
}

json write_station(const proto::Station& s, SourceFormat fmt) {
    json j;
    j["instanceName"] = s.id();
    j["className"] = s.class_name();
    j["pos"] = point3_to_json(s.pose().position(), /*include_z=*/fmt == SourceFormat::Rbk35);
    j["dir"] = rad_to_dir(s.pose().heading_rad(), fmt);
    if (s.ignore_heading()) j["ignoreDir"] = true;
    j["desc"] = std::string{s.description().data(), s.description().size()};
    j["property"] = write_properties(s.properties());
    return j;
}

// ---- Path ----

constexpr const char* kClassStraight = "StraightPath";
constexpr const char* kClassDegenerateBezier = "DegenerateBezier";

proto::Path read_path(const json& j, const StationLookup& by_id) {
    proto::Path p;
    if (auto it = j.find("instanceName"); it != j.end() && it->is_string()) {
        p.set_id(it->get<std::string>());
        p.set_name(it->get<std::string>());
    }
    std::string class_name;
    if (auto it = j.find("className"); it != j.end() && it->is_string()) {
        class_name = it->get<std::string>();
        p.set_class_name(class_name);
    }

    // rbk34 stores startPos/endPos as objects {instanceName, pos}; rbk35
    // stores them as bare strings. Either way we treat the station's pose as
    // authoritative when available, falling back to the embedded pos.
    auto resolve_endpoint = [&](const json& v, std::string& out_id) -> proto::Point3 {
        if (v.is_string()) {
            out_id = v.get<std::string>();
        } else if (v.is_object()) {
            if (auto it = v.find("instanceName"); it != v.end() && it->is_string()) {
                out_id = it->get<std::string>();
            }
        }
        if (auto it = by_id.find(out_id); it != by_id.end()) {
            return it->second->pose().position();
        }
        if (v.is_object()) {
            if (auto pos = v.find("pos"); pos != v.end()) {
                return read_point3(*pos);
            }
        }
        return {};
    };

    std::string start_id;
    std::string end_id;
    proto::Point3 start_pt;
    proto::Point3 end_pt;
    if (auto it = j.find("startPos"); it != j.end()) start_pt = resolve_endpoint(*it, start_id);
    if (auto it = j.find("endPos"); it != j.end()) end_pt = resolve_endpoint(*it, end_id);
    p.set_start_station_id(start_id);
    p.set_end_station_id(end_id);

    auto* g = p.mutable_geometry();
    if (class_name == kClassStraight) {
        g->set_kind(proto::CURVE_KIND_LINE_SEGMENT);
        g->set_degree(1);
        *g->add_control_points() = start_pt;
        *g->add_control_points() = end_pt;
    } else if (class_name == kClassDegenerateBezier) {
        proto::Point3 cp1 = j.contains("controlPos1") ? read_point3(j["controlPos1"]) : proto::Point3{};
        proto::Point3 cp2 = j.contains("controlPos2") ? read_point3(j["controlPos2"]) : proto::Point3{};
        g->set_kind(proto::CURVE_KIND_BEZIER);
        g->set_degree(5);
        *g->add_control_points() = start_pt;
        *g->add_control_points() = cp1;
        *g->add_control_points() = cp1;
        *g->add_control_points() = cp2;
        *g->add_control_points() = cp2;
        *g->add_control_points() = end_pt;
    } else {
        // Unknown class: keep endpoints so downstream consistency checks pass.
        g->set_kind(proto::CURVE_KIND_OTHER);
        *g->add_control_points() = start_pt;
        *g->add_control_points() = end_pt;
    }

    if (auto it = j.find("desc"); it != j.end() && it->is_string()) {
        const auto& d = it->get_ref<const std::string&>();
        p.set_description(d.data(), d.size());
    }
    if (auto it = j.find("property"); it != j.end()) {
        read_properties(*it, *p.mutable_properties());
    }
    return p;
}

json write_path(const proto::Path& p, const StationLookup& by_id, SourceFormat fmt) {
    json j;
    j["instanceName"] = p.id();
    j["className"] = p.class_name();

    auto endpoint = [&](const std::string& sid, const proto::Point3& fallback) -> json {
        if (fmt == SourceFormat::Rbk35) {
            return sid;
        }
        json e{{"instanceName", sid}};
        if (auto it = by_id.find(sid); it != by_id.end()) {
            e["pos"] = point3_to_json(it->second->pose().position(), /*include_z=*/false);
        } else {
            e["pos"] = point3_to_json(fallback, /*include_z=*/false);
        }
        return e;
    };

    const auto& cps = p.geometry().control_points();
    proto::Point3 first = cps.size() > 0 ? cps.Get(0) : proto::Point3{};
    proto::Point3 last = cps.size() > 0 ? cps.Get(cps.size() - 1) : proto::Point3{};
    j["startPos"] = endpoint(p.start_station_id(), first);
    j["endPos"] = endpoint(p.end_station_id(), last);

    if (p.class_name() == kClassDegenerateBezier && cps.size() == 6) {
        j["controlPos1"] = point3_to_json(cps.Get(1), /*include_z=*/fmt == SourceFormat::Rbk35);
        j["controlPos2"] = point3_to_json(cps.Get(3), /*include_z=*/fmt == SourceFormat::Rbk35);
    }

    j["desc"] = std::string{p.description().data(), p.description().size()};
    j["property"] = write_properties(p.properties());
    return j;
}

// ---- smap JSON ↔ semantic Map ----

void parse_smap_json(const json& doc, SourceFormat fmt, proto::MapPackage& pkg) {
    auto* m = pkg.mutable_map();

    if (auto it = doc.find("header"); it != doc.end() && it->is_object()) {
        const auto& h = *it;
        auto* hdr = m->mutable_header();
        if (auto k = h.find("mapName"); k != h.end() && k->is_string()) hdr->set_name(k->get<std::string>());
        if (auto k = h.find("mapType"); k != h.end() && k->is_string()) hdr->set_map_type(k->get<std::string>());
        if (auto k = h.find("version"); k != h.end() && k->is_string()) hdr->set_source_version(k->get<std::string>());
        if (auto k = h.find("resolution"); k != h.end() && k->is_number()) hdr->set_resolution_m(k->get<double>());
        if (auto k = h.find("minPos"); k != h.end()) {
            *hdr->mutable_bounds()->mutable_min() = read_point3(*k);
        }
        if (auto k = h.find("maxPos"); k != h.end()) {
            *hdr->mutable_bounds()->mutable_max() = read_point3(*k);
        }
    }

    if (auto it = doc.find("advancedPointList"); it != doc.end() && it->is_array()) {
        for (const auto& entry : *it) {
            *m->add_stations() = read_station(entry, fmt);
        }
    }

    StationLookup by_id;
    for (const auto& s : m->stations()) {
        if (!s.id().empty()) by_id.emplace(s.id(), &s);
    }

    if (auto it = doc.find("advancedCurveList"); it != doc.end() && it->is_array()) {
        for (const auto& entry : *it) {
            *m->add_paths() = read_path(entry, by_id);
        }
    }
}

json render_smap_json(const proto::MapPackage& pkg, SourceFormat fmt, const json& base) {
    // Start from the source-bundle copy of the original .smap so we preserve
    // every top-level key we don't model semantically. We then overwrite the
    // sections owned by the wrap layer (header / advancedPointList /
    // advancedCurveList).
    json out = base.is_object() ? base : json::object();

    const auto& m = pkg.map();
    const auto& hdr = m.header();

    json h = (out.contains("header") && out["header"].is_object()) ? out["header"] : json::object();
    if (!hdr.name().empty()) h["mapName"] = hdr.name();
    if (!hdr.map_type().empty()) h["mapType"] = hdr.map_type();
    if (!hdr.source_version().empty()) h["version"] = hdr.source_version();
    if (hdr.resolution_m() != 0.0) h["resolution"] = hdr.resolution_m();
    if (hdr.has_bounds()) {
        h["minPos"] = point3_to_json(hdr.bounds().min(), /*include_z=*/fmt == SourceFormat::Rbk35);
        h["maxPos"] = point3_to_json(hdr.bounds().max(), /*include_z=*/fmt == SourceFormat::Rbk35);
    }
    out["header"] = std::move(h);

    auto stations_arr = json::array();
    for (const auto& s : m.stations()) {
        stations_arr.push_back(write_station(s, fmt));
    }
    out["advancedPointList"] = std::move(stations_arr);

    StationLookup by_id;
    for (const auto& s : m.stations()) {
        if (!s.id().empty()) by_id.emplace(s.id(), &s);
    }
    auto paths_arr = json::array();
    for (const auto& p : m.paths()) {
        paths_arr.push_back(write_path(p, by_id, fmt));
    }
    out["advancedCurveList"] = std::move(paths_arr);

    return out;
}

// ---- SourceBundle helpers ----

const proto::SourceFile* find_source_file(const proto::MapPackage& pkg, proto::SourceFileRole role) {
    for (const auto& f : pkg.source().files()) {
        if (f.role() == role) return &f;
    }
    return nullptr;
}

void add_source_file(proto::MapPackage& pkg, const fs::path& rel_path, proto::SourceFileRole role, std::string_view media_type, std::string bytes) {
    auto* f = pkg.mutable_source()->add_files();
    f->set_path(rel_path.generic_string());
    f->set_role(role);
    f->set_media_type(std::string{media_type});
    f->set_data(std::move(bytes));
}

// ---- rbk34 ----

std::expected<proto::MapPackage, Error> load_rbk34(const fs::path& path) {
    auto bytes = read_file(path);
    if (!bytes) return std::unexpected{std::move(bytes).error()};

    auto doc = parse_json(*bytes, "rbk34 smap");
    if (!doc) return std::unexpected{std::move(doc).error()};

    proto::MapPackage pkg;
    pkg.set_ir_schema_version("1");
    pkg.set_source_format("rbk34");
    parse_smap_json(*doc, SourceFormat::Rbk34, pkg);
    add_source_file(pkg, path.filename(), proto::SOURCE_FILE_ROLE_PRIMARY_MAP_JSON, "application/json", std::move(*bytes));
    return pkg;
}

std::expected<void, Error> save_rbk34(const proto::MapPackage& pkg, const fs::path& path, const SaveOptions& opt) {
    if (!opt.overwrite && target_is_nonempty(path)) {
        return std::unexpected{Error{Error::Code::TargetExists, "target file exists and is non-empty: " + path.string()}};
    }
    json base;
    if (const auto* src = find_source_file(pkg, proto::SOURCE_FILE_ROLE_PRIMARY_MAP_JSON)) {
        auto parsed = parse_json(src->data(), "rbk34 source bundle");
        if (parsed) base = std::move(*parsed);
    }
    json out = render_smap_json(pkg, SourceFormat::Rbk34, base);

    std::error_code ec;
    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path(), ec);
    }
    return write_file(path, out.dump());
}

// ---- rbk35 ----

std::expected<proto::MapPackage, Error> load_rbk35(const fs::path& dir) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return std::unexpected{Error{Error::Code::FileNotFound, "not a directory: " + dir.string()}};
    }
    proto::MapPackage pkg;
    pkg.set_ir_schema_version("1");
    pkg.set_source_format("rbk35");

    if (const fs::path info_path = dir / "info.json"; fs::exists(info_path, ec)) {
        auto bytes = read_file(info_path);
        if (!bytes) return std::unexpected{std::move(bytes).error()};
        add_source_file(pkg, "info.json", proto::SOURCE_FILE_ROLE_FOLDER_INFO_JSON, "application/json", std::move(*bytes));
    }

    // Pick the alphabetically smallest .smap file as the primary one.
    fs::path smap_path;
    for (const auto& e : fs::directory_iterator{dir, ec}) {
        if (e.is_regular_file() && e.path().extension() == ".smap") {
            if (smap_path.empty() || e.path() < smap_path) {
                smap_path = e.path();
            }
        }
    }
    if (smap_path.empty()) {
        return std::unexpected{Error{Error::Code::ParseFailed, "no .smap file in: " + dir.string()}};
    }
    auto smap_bytes = read_file(smap_path);
    if (!smap_bytes) return std::unexpected{std::move(smap_bytes).error()};
    auto doc = parse_json(*smap_bytes, "rbk35 smap");
    if (!doc) return std::unexpected{std::move(doc).error()};
    parse_smap_json(*doc, SourceFormat::Rbk35, pkg);
    add_source_file(pkg, smap_path.filename(), proto::SOURCE_FILE_ROLE_PRIMARY_MAP_JSON, "application/json", std::move(*smap_bytes));

    // Stash the binary tile subdirectories verbatim; their contents are
    // protobuf-serialized chunks that this v1 codec does not parse.
    auto load_tile_dir = [&](const fs::path& sub) -> std::expected<void, Error> {
        const fs::path d = dir / sub;
        if (!fs::is_directory(d, ec)) return {};
        for (const auto& e : fs::directory_iterator{d, ec}) {
            if (!e.is_regular_file()) continue;
            auto bytes = read_file(e.path());
            if (!bytes) return std::unexpected{std::move(bytes).error()};
            // info.json sitting inside a tile dir is a per-folder info file,
            // not a folder-level info; mark it separately.
            const proto::SourceFileRole role =
                (e.path().filename() == "info.json")
                    ? proto::SOURCE_FILE_ROLE_LAYER_INFO_JSON
                    : proto::SOURCE_FILE_ROLE_RBK_TILE;
            const std::string_view media =
                (role == proto::SOURCE_FILE_ROLE_LAYER_INFO_JSON)
                    ? "application/json"
                    : "application/octet-stream";
            add_source_file(pkg, sub / e.path().filename(), role, media, std::move(*bytes));
        }
        return {};
    };
    if (auto r = load_tile_dir("2dft"); !r) return std::unexpected{r.error()};
    if (auto r = load_tile_dir("2dlh"); !r) return std::unexpected{r.error()};
    if (auto r = load_tile_dir("2dpc"); !r) return std::unexpected{r.error()};

    return pkg;
}

std::expected<void, Error> save_rbk35(const proto::MapPackage& pkg, const fs::path& dir, const SaveOptions& opt) {
    std::error_code ec;
    if (!opt.overwrite && target_is_nonempty(dir)) {
        return std::unexpected{Error{Error::Code::TargetExists, "target directory exists and is non-empty: " + dir.string()}};
    }
    fs::create_directories(dir, ec);
    if (ec) {
        return std::unexpected{Error{Error::Code::IoError, "cannot create directory: " + dir.string()}};
    }

    std::string smap_rel = "0.smap";
    json base;
    for (const auto& f : pkg.source().files()) {
        if (f.role() == proto::SOURCE_FILE_ROLE_PRIMARY_MAP_JSON) {
            if (!f.path().empty()) smap_rel = f.path();
            auto parsed = parse_json(f.data(), "rbk35 source bundle");
            if (parsed) base = std::move(*parsed);
            break;
        }
    }
    json out = render_smap_json(pkg, SourceFormat::Rbk35, base);

    fs::path smap_out = dir / smap_rel;
    if (smap_out.has_parent_path()) {
        fs::create_directories(smap_out.parent_path(), ec);
    }
    if (auto r = write_file(smap_out, out.dump()); !r) return std::unexpected{r.error()};

    // Write every other stashed source file at its original relative path.
    for (const auto& f : pkg.source().files()) {
        if (f.role() == proto::SOURCE_FILE_ROLE_PRIMARY_MAP_JSON) continue;
        if (f.path().empty()) continue;
        fs::path target = dir / f.path();
        if (target.has_parent_path()) {
            fs::create_directories(target.parent_path(), ec);
        }
        if (auto r = write_file(target, f.data()); !r) return std::unexpected{r.error()};
    }

    return {};
}

}  // namespace

std::expected<proto::MapPackage, Error> load(const std::filesystem::path& path, SourceFormat format) {
    switch (format) {
        case SourceFormat::Rbk34: return load_rbk34(path);
        case SourceFormat::Rbk35: return load_rbk35(path);
    }
    return std::unexpected{Error{Error::Code::InvalidArgument, "unknown SourceFormat"}};
}

std::expected<void, Error> save(const proto::MapPackage& package, const std::filesystem::path& path, SourceFormat format, const SaveOptions& options) {
    switch (format) {
        case SourceFormat::Rbk34: return save_rbk34(package, path, options);
        case SourceFormat::Rbk35: return save_rbk35(package, path, options);
    }
    return std::unexpected{Error{Error::Code::InvalidArgument, "unknown SourceFormat"}};
}

}  // namespace smap1::codec
