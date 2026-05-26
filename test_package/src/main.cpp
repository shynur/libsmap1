#include <smap1.hpp>
#include "print_pkg_info.hpp"

#include <cstdlib>
#include <print>
#include <utility>

namespace {

smap1::proto::Station make_station(std::string id, double x, double y) {
    smap1::proto::Station s;
    s.set_id(std::move(id));
    s.set_class_name("LocationMark");
    auto* pos = s.mutable_pose()->mutable_position();
    pos->set_x(x);
    pos->set_y(y);
    return s;
}

void demo_wrap() {
    smap1::Map map{smap1::proto::MapPackage{}};

    [[maybe_unused]] auto a = map.add_station(make_station("LM1", 0.0, 0.0));
    [[maybe_unused]] auto b = map.add_station(make_station("LM2", 1.0, 0.0));

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
        std::println("add_path failed: {}", added.error().message);
        return;
    }

    std::println("wrap: stations={}, paths={}, issues={}",
                 map.station_count(), map.path_count(), smap1::validate(map).size());

    if (auto r = map.add_station(make_station("LM1", 9.0, 9.0)); !r) {
        std::println("wrap: expected duplicate-id error: {}", r.error().message);
    }
}

void demo_codec(const char* examples_root) {
    if (examples_root == nullptr) {
        std::println("codec: SMAP1_EXAMPLES not set, skipping codec smoke");
        return;
    }
    const std::string root = examples_root;

    const std::string rbk34_path = root + "/rbk34/raw-json.smap";
    if (auto pkg = smap1::codec::load(rbk34_path, smap1::codec::SourceFormat::Rbk34)) {
        smap1::Map m{std::move(*pkg)};
        std::println("codec rbk34: stations={}, paths={}, issues={}",
                     m.station_count(), m.path_count(), smap1::validate(m).size());
    } else {
        std::println("codec rbk34 load failed: {}", pkg.error().message);
    }

    const std::string rbk35_dir = root + "/rbk35/raw-folder";
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

}  // namespace

int main() {
    print_pkg_info();
    smap1::hello_json();
    smap1::hello_protobuf();
    demo_wrap();
    demo_codec(std::getenv("SMAP1_EXAMPLES"));
}
