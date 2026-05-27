#include <smap1/map.hpp>

#include "log_internal.hpp"

#include <cmath>
#include <string>
#include <utility>

namespace smap1 {

namespace {

// Endpoint comparison tolerance, in meters. Stations cannot move; control
// points that come from station poses should match exactly, but we allow a
// tiny epsilon to absorb float/double round-trips through JSON.
constexpr double kPointEpsilon = 1e-6;

bool points_equal(const proto::Point3& a, const proto::Point3& b) {
    return std::abs(a.x() - b.x()) <= kPointEpsilon
        && std::abs(a.y() - b.y()) <= kPointEpsilon
        && std::abs(a.z() - b.z()) <= kPointEpsilon;
}

int station_index(const proto::MapPackage& pkg, std::string_view id) {
    const auto& v = pkg.map().stations();
    for (int i = 0; i < v.size(); ++i) {
        if (v.Get(i).id() == id) {
            return i;
        }
    }
    return -1;
}

int path_index(const proto::MapPackage& pkg, std::string_view id) {
    const auto& v = pkg.map().paths();
    for (int i = 0; i < v.size(); ++i) {
        if (v.Get(i).id() == id) {
            return i;
        }
    }
    return -1;
}

}  // namespace

Map::Map(proto::MapPackage package) noexcept
    : package_(std::move(package)) {}

const proto::MapPackage& Map::pb() const& noexcept { return package_; }
proto::MapPackage& Map::pb() & noexcept { return package_; }
proto::MapPackage Map::into_pb() && noexcept { return std::move(package_); }

// ---------------- Station ----------------

int Map::station_count() const noexcept {
    return package_.map().stations_size();
}

const proto::Station* Map::find_station(std::string_view id) const noexcept {
    const int i = station_index(package_, id);
    return i < 0 ? nullptr : &package_.map().stations(i);
}

proto::Station* Map::find_station(std::string_view id) noexcept {
    const int i = station_index(package_, id);
    return i < 0 ? nullptr : package_.mutable_map()->mutable_stations(i);
}

std::expected<proto::Station*, Error> Map::add_station(proto::Station station) {
    if (station.id().empty()) {
        SMAP1_LOG_WARN("Map.add_station rejected: empty id");
        return std::unexpected{Error{
            Error::Code::InvalidArgument,
            "station.id must be non-empty",
        }};
    }
    if (station_index(package_, station.id()) >= 0) {
        SMAP1_LOG_WARN("Map.add_station rejected: id '{}' already exists", station.id());
        return std::unexpected{Error{
            Error::Code::IdConflict,
            "station id '" + station.id() + "' already exists",
        }};
    }
    auto* slot = package_.mutable_map()->add_stations();
    *slot = std::move(station);
    SMAP1_LOG_DEBUG("Map.add_station ok: id='{}', total={}", slot->id(), package_.map().stations_size());
    return slot;
}

bool Map::remove_station(std::string_view id) {
    const int idx = station_index(package_, id);
    if (idx < 0) {
        SMAP1_LOG_DEBUG("Map.remove_station miss: id='{}'", id);
        return false;
    }
    const std::string id_copy{id};

    // Cascade: drop every path that references this station as start or end.
    // Iterate from the back so DeleteSubrange indices remain valid.
    auto* paths = package_.mutable_map()->mutable_paths();
    int cascaded = 0;
    for (int i = paths->size() - 1; i >= 0; --i) {
        const auto& p = paths->Get(i);
        if (p.start_station_id() == id_copy || p.end_station_id() == id_copy) {
            paths->DeleteSubrange(i, 1);
            ++cascaded;
        }
    }

    package_.mutable_map()->mutable_stations()->DeleteSubrange(idx, 1);
    SMAP1_LOG_DEBUG("Map.remove_station ok: id='{}', cascaded {} path(s)", id_copy, cascaded);
    return true;
}

// ---------------- Path ----------------

int Map::path_count() const noexcept {
    return package_.map().paths_size();
}

const proto::Path* Map::find_path(std::string_view id) const noexcept {
    const int i = path_index(package_, id);
    return i < 0 ? nullptr : &package_.map().paths(i);
}

proto::Path* Map::find_path(std::string_view id) noexcept {
    const int i = path_index(package_, id);
    return i < 0 ? nullptr : package_.mutable_map()->mutable_paths(i);
}

std::vector<const proto::Path*> Map::find_paths_between(std::string_view start_station_id, std::string_view end_station_id) const {
    std::vector<const proto::Path*> out;
    for (const auto& p : package_.map().paths()) {
        if (p.start_station_id() == start_station_id
            && p.end_station_id() == end_station_id) {
            out.push_back(&p);
        }
    }
    return out;
}

std::vector<proto::Path*> Map::find_paths_between(std::string_view start_station_id, std::string_view end_station_id) {
    std::vector<proto::Path*> out;
    auto* paths = package_.mutable_map()->mutable_paths();
    for (int i = 0; i < paths->size(); ++i) {
        auto* p = paths->Mutable(i);
        if (p->start_station_id() == start_station_id
            && p->end_station_id() == end_station_id) {
            out.push_back(p);
        }
    }
    return out;
}

std::expected<proto::Path*, Error> Map::add_path(proto::Path path) {
    if (path.id().empty()) {
        SMAP1_LOG_WARN("Map.add_path rejected: empty id");
        return std::unexpected{Error{
            Error::Code::InvalidArgument,
            "path.id must be non-empty",
        }};
    }
    if (path_index(package_, path.id()) >= 0) {
        SMAP1_LOG_WARN("Map.add_path rejected: id '{}' already exists", path.id());
        return std::unexpected{Error{
            Error::Code::IdConflict,
            "path id '" + path.id() + "' already exists",
        }};
    }

    const proto::Station* start = find_station(path.start_station_id());
    if (start == nullptr) {
        SMAP1_LOG_WARN("Map.add_path rejected: id='{}' missing start station '{}'",
            path.id(), path.start_station_id());
        return std::unexpected{Error{
            Error::Code::IdNotFound,
            "path '" + path.id() + "' references missing start station '"
                + path.start_station_id() + "'",
        }};
    }
    const proto::Station* end = find_station(path.end_station_id());
    if (end == nullptr) {
        SMAP1_LOG_WARN("Map.add_path rejected: id='{}' missing end station '{}'",
            path.id(), path.end_station_id());
        return std::unexpected{Error{
            Error::Code::IdNotFound,
            "path '" + path.id() + "' references missing end station '"
                + path.end_station_id() + "'",
        }};
    }

    const auto& cps = path.geometry().control_points();
    if (cps.empty()) {
        SMAP1_LOG_WARN("Map.add_path rejected: id='{}' empty control_points", path.id());
        return std::unexpected{Error{
            Error::Code::InvalidArgument,
            "path '" + path.id() + "' has empty geometry.control_points",
        }};
    }
    if (!points_equal(cps.Get(0), start->pose().position())) {
        SMAP1_LOG_WARN("Map.add_path rejected: id='{}' first control point != start station pose", path.id());
        return std::unexpected{Error{
            Error::Code::EndpointMismatch,
            "path '" + path.id() + "': first control point does not match start station pose",
        }};
    }
    if (!points_equal(cps.Get(cps.size() - 1), end->pose().position())) {
        SMAP1_LOG_WARN("Map.add_path rejected: id='{}' last control point != end station pose", path.id());
        return std::unexpected{Error{
            Error::Code::EndpointMismatch,
            "path '" + path.id() + "': last control point does not match end station pose",
        }};
    }

    auto* slot = package_.mutable_map()->add_paths();
    *slot = std::move(path);
    SMAP1_LOG_DEBUG("Map.add_path ok: id='{}', total={}", slot->id(), package_.map().paths_size());
    return slot;
}

bool Map::remove_path(std::string_view id) {
    const int idx = path_index(package_, id);
    if (idx < 0) {
        SMAP1_LOG_DEBUG("Map.remove_path miss: id='{}'", id);
        return false;
    }
    package_.mutable_map()->mutable_paths()->DeleteSubrange(idx, 1);
    SMAP1_LOG_DEBUG("Map.remove_path ok: id='{}'", id);
    return true;
}

}  // namespace smap1
