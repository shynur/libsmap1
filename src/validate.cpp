#include <smap1/validate.hpp>

#include <smap1/map.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace smap1 {

namespace {

constexpr double kPointEpsilon = 1e-6;

bool points_equal(const proto::Point3& a, const proto::Point3& b) {
    return std::abs(a.x() - b.x()) <= kPointEpsilon
        && std::abs(a.y() - b.y()) <= kPointEpsilon
        && std::abs(a.z() - b.z()) <= kPointEpsilon;
}

void check_stations(const proto::Map& m, std::vector<Issue>& out) {
    std::unordered_set<std::string> seen;
    for (const auto& s : m.stations()) {
        if (s.id().empty()) {
            out.push_back({Issue::Kind::InvalidField, "Station", "", "station has empty id"});
            continue;
        }
        if (!seen.insert(s.id()).second) {
            out.push_back({Issue::Kind::DuplicateId, "Station", s.id(), "duplicate station id"});
        }
    }
}

void check_paths(const proto::Map& m, std::vector<Issue>& out) {
    // Build a lookup once; paths often reference each station many times.
    std::unordered_map<std::string, const proto::Station*> by_id;
    for (const auto& s : m.stations()) {
        if (!s.id().empty()) {
            by_id.emplace(s.id(), &s);
        }
    }

    std::unordered_set<std::string> seen;
    for (const auto& p : m.paths()) {
        if (p.id().empty()) {
            out.push_back({Issue::Kind::InvalidField, "Path", "", "path has empty id"});
        } else if (!seen.insert(p.id()).second) {
            out.push_back({Issue::Kind::DuplicateId, "Path", p.id(), "duplicate path id"});
        }

        const proto::Station* start = nullptr;
        const proto::Station* end = nullptr;
        if (auto it = by_id.find(p.start_station_id()); it != by_id.end()) {
            start = it->second;
        }
        if (auto it = by_id.find(p.end_station_id()); it != by_id.end()) {
            end = it->second;
        }
        if (start == nullptr) {
            out.push_back({Issue::Kind::DanglingStationRef, "Path", p.id(),
                "path start_station_id '" + p.start_station_id() + "' does not exist"});
        }
        if (end == nullptr) {
            out.push_back({Issue::Kind::DanglingStationRef, "Path", p.id(),
                "path end_station_id '" + p.end_station_id() + "' does not exist"});
        }

        const auto& g = p.geometry();
        const auto& cps = g.control_points();
        if (g.kind() == proto::CURVE_KIND_BEZIER && cps.size() > 0) {
            if (static_cast<std::uint32_t>(cps.size()) != g.degree() + 1u) {
                out.push_back({Issue::Kind::ControlPointCountMismatch, "Path", p.id(),
                    "bezier control_points count does not equal degree + 1"});
            }
        }

        if (start != nullptr && cps.size() >= 1) {
            if (!points_equal(cps.Get(0), start->pose().position())) {
                out.push_back({Issue::Kind::EndpointMismatch, "Path", p.id(),
                    "first control point does not match start station pose"});
            }
        }
        if (end != nullptr && cps.size() >= 1) {
            if (!points_equal(cps.Get(cps.size() - 1), end->pose().position())) {
                out.push_back({Issue::Kind::EndpointMismatch, "Path", p.id(),
                    "last control point does not match end station pose"});
            }
        }
    }
}

}  // namespace

std::vector<Issue> validate(const proto::MapPackage& package) {
    std::vector<Issue> issues;
    const auto& m = package.map();
    check_stations(m, issues);
    check_paths(m, issues);
    return issues;
}

std::vector<Issue> validate(const Map& map) {
    return validate(map.pb());
}

}  // namespace smap1
