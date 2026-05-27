#pragma once

#include <smap1.hpp>

#include <print>
#include <stdexcept>
#include <string>
#include <utility>

inline smap1::proto::Station make_station(std::string id, double x, double y) {
    smap1::proto::Station s;
    s.set_id(std::move(id));
    s.set_class_name("LocationMark");
    auto *pos = s.mutable_pose()->mutable_position();
    pos->set_x(x);
    pos->set_y(y);
    return s;
}

inline smap1::Map load_then_edit(const char *src_path, smap1::codec::SourceFormat src_format) {
    auto pkg = smap1::codec::load(src_path, src_format);
    if (!pkg) {
        throw std::runtime_error{"wrap load failed: " + pkg.error().message};
    }
    smap1::Map map{std::move(*pkg)};

    auto added_lm1 [[maybe_unused]] = map.add_station(make_station("LM1", 0.0, 0.0));
    auto added_lm2 [[maybe_unused]] = map.add_station(make_station("LM2", 1.0, 0.0));

    smap1::proto::Path path;
    path.set_id("LM1-LM2");
    path.set_class_name("StraightPath");
    path.set_start_station_id("LM1");
    path.set_end_station_id("LM2");
    *path.mutable_geometry() = smap1::geometry::make_line_segment(
        map.find_station("LM1")->pose().position(),
        map.find_station("LM2")->pose().position()
    );

    if (auto added = map.add_path(std::move(path)); !added) {
        throw std::runtime_error{"wrap add_path failed: " + added.error().message};
    }

    std::println("wrap: stations={}, paths={}, issues={}",
                 map.station_count(), map.path_count(), smap1::validate(map).size());

    if (auto r = map.add_station(make_station("LM1", 9.0, 9.0)); !r) {
        std::println("wrap: expected duplicate-id error: {}", r.error().message);
    }

    return map;
}
