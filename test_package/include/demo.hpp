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
    auto* pos = s.mutable_pose()->mutable_position();
    pos->set_x(x);
    pos->set_y(y);
    return s;
}

inline void demo_wrap(const char* rbk34_path) {
    auto pkg = smap1::codec::load(rbk34_path, smap1::codec::SourceFormat::Rbk34);
    if (!pkg) {
        throw std::runtime_error{"wrap rbk34 load failed: " + pkg.error().message};
    }
    smap1::Map map_rbk34{std::move(*pkg)};

    [[maybe_unused]] auto a = map_rbk34.add_station(make_station("LM1", 0.0, 0.0));
    [[maybe_unused]] auto b = map_rbk34.add_station(make_station("LM2", 1.0, 0.0));

    smap1::proto::Path path;
    path.set_id("LM1-LM2");
    path.set_class_name("StraightPath");
    path.set_start_station_id("LM1");
    path.set_end_station_id("LM2");
    *path.mutable_geometry() = smap1::geometry::make_line_segment(
        map_rbk34.find_station("LM1")->pose().position(),
        map_rbk34.find_station("LM2")->pose().position()
    );

    if (auto added = map_rbk34.add_path(std::move(path)); !added) {
        std::println("add_path failed: {}", added.error().message);
        return;
    }

    std::println("wrap: stations={}, paths={}, issues={}",
                 map_rbk34.station_count(), map_rbk34.path_count(), smap1::validate(map_rbk34).size());

    if (auto r = map_rbk34.add_station(make_station("LM1", 9.0, 9.0)); !r) {
        std::println("wrap: expected duplicate-id error: {}", r.error().message);
    }
}

inline void demo_codec(const char* rbk34_path, const char* rbk35_dir) {
    if (rbk34_path == nullptr || rbk35_dir == nullptr) {
        std::println("codec: rbk34/rbk35 path not set, skipping codec smoke");
        return;
    }

    if (auto pkg = smap1::codec::load(rbk34_path, smap1::codec::SourceFormat::Rbk34)) {
        smap1::Map m{std::move(*pkg)};
        std::println("codec rbk34: stations={}, paths={}, issues={}",
                     m.station_count(), m.path_count(), smap1::validate(m).size());
    } else {
        std::println("codec rbk34 load failed: {}", pkg.error().message);
    }

    if (auto pkg = smap1::codec::load(rbk35_dir, smap1::codec::SourceFormat::Rbk35)) {
        smap1::Map m{std::move(*pkg)};
        std::println("codec rbk35: stations={}, paths={}, issues={}",
                     m.station_count(), m.path_count(), smap1::validate(m).size());

        const std::string rt_dir = "/tmp/smap1_rt_rbk35";
        if (auto sv = smap1::codec::save(m.pb(), rt_dir,
                                          smap1::codec::SourceFormat::Rbk35,
                                          smap1::codec::SaveOptions{.overwrite = true})) {
            if (auto pkg2 = smap1::codec::load(rt_dir, smap1::codec::SourceFormat::Rbk35)) {
                smap1::Map m2{std::move(*pkg2)};
                std::println("codec rbk35 round-trip: stations={}, paths={}, issues={}",
                             m2.station_count(), m2.path_count(), smap1::validate(m2).size());
            } else {
                std::println("codec rbk35 reload failed: {}", pkg2.error().message);
            }
        } else {
            std::println("codec rbk35 save failed: {}", sv.error().message);
        }
    } else {
        std::println("codec rbk35 load failed: {}", pkg.error().message);
    }
}
