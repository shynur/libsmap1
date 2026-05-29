#pragma once

#include <smap1.hpp>

#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
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

inline std::string_view shape_kind(const smap1::proto::RobotModel& m) {
    switch (m.shape_case()) {
        case smap1::proto::RobotModel::kRectangle:
            return "rectangle";
        case smap1::proto::RobotModel::kCircle:
            return "circle";
        case smap1::proto::RobotModel::kPolygon:
            return "polygon";
        case smap1::proto::RobotModel::SHAPE_NOT_SET:
            return "<unset>";
    }
    return "?";
}

// 加载 robot model 文件并打印关心的字段.
inline smap1::proto::RobotModel load_robot_model_or_throw(const char *path,
                                                          smap1::codec::RobotModelFormat fmt) {
    auto rm = smap1::codec::load_robot_model(path, fmt);
    if (!rm) {
        throw std::runtime_error{"加载 robot model 失败: " + rm.error().message};
    }
    const auto kind = shape_kind(*rm);
    std::println("已加载 robot model: id='{}' shape={} (来源: {})",
                 rm->model_id(), kind, rm->source_path());
    if (rm->shape_case() == smap1::proto::RobotModel::kRectangle) {
        const auto& r = rm->rectangle();
        std::println("  rectangle: width={:.3f} head={:.3f} tail={:.3f} height={:.3f}",
                     r.width(), r.head(), r.tail(), r.height());
    } else if (rm->shape_case() == smap1::proto::RobotModel::kCircle) {
        const auto& c = rm->circle();
        std::println("  circle: radius={:.3f} height={:.3f}", c.radius(), c.height());
    } else if (rm->shape_case() == smap1::proto::RobotModel::kPolygon) {
        std::println("  polygon: {} vertices, height={:.3f}",
                     rm->polygon().vertices_size(), rm->polygon().height());
    }
    const auto& s2c = rm->shape_to_chassis();
    std::println("  shape_to_chassis: x={:.3f} y={:.3f} heading_rad={:.3f}",
                 s2c.position().x(), s2c.position().y(), s2c.heading_rad());
    return std::move(*rm);
}

inline smap1::Map load_then_edit(const char *src_path, smap1::codec::SourceFormat src_format) {
    auto pkg = smap1::codec::load(src_path, src_format);
    if (!pkg) {
        throw std::runtime_error{"加载地图失败: " + pkg.error().message};
    }
    smap1::Map map{std::move(*pkg)};

    auto added_lm1 [[maybe_unused]] = map.add_station(make_station("demo-LM1", 0.0, 0.0));
    auto added_lm2 [[maybe_unused]] = map.add_station(make_station("demo-LM2", 1.0, 0.0));

    smap1::proto::Path path;
    path.set_id("demo-LM1-LM2");
    path.set_class_name("StraightPath");
    path.set_start_station_id("demo-LM1");
    path.set_end_station_id("demo-LM2");
    *path.mutable_geometry() = smap1::geometry::make_line_segment(
        map.find_station("demo-LM1")->pose().position(),
        map.find_station("demo-LM2")->pose().position()
    );

    if (auto added = map.add_path(std::move(path)); !added) {
        throw std::runtime_error{"添加路径失败: " + added.error().message};
    }

    std::println("已加载并编辑地图: 站点 {} 个, 路径 {} 条, 校验问题 {} 处",
                 map.station_count(), map.path_count(), smap1::validate(map).size());

    if (auto r = map.add_station(make_station("demo-LM1", 9.0, 9.0)); !r) {
        std::println("已按预期触发重复 ID 校验: {}", r.error().message);
    }

    return map;
}

// 把 robot_model 与一张 map 串起来, 演示三件事:
//   1. set_robot_model 后能查到当前关联的 model_id.
//   2. footprint_at 把 shape 顶点变换到指定 pose 的 map 坐标系.
//   3. add_station 在已知 footprint + bounds 的情况下, 拒绝越界的站点.
inline void demo_robot_model_with_map(smap1::Map& map, smap1::proto::RobotModel model) {
    map.set_robot_model(std::move(model));
    std::println("set_robot_model: map.robot_model()->model_id() = '{}'",
                 map.robot_model() ? map.robot_model()->model_id() : "<null>");

    // 在地图原点处计算 footprint.
    smap1::proto::Pose2D origin;
    origin.mutable_position()->set_x(0.0);
    origin.mutable_position()->set_y(0.0);
    origin.set_heading_rad(0.0);
    const auto fp_origin = map.footprint_at(origin);
    std::println("footprint_at origin: {} 个顶点", fp_origin.vertices_size());
    for (int i = 0; i < fp_origin.vertices_size() && i < 4; ++i) {
        const auto& v = fp_origin.vertices(i);
        std::println("  v[{}] = ({:.3f}, {:.3f})", i, v.x(), v.y());
    }
    std::println("  check_in_bounds(origin footprint) = {}",
                 map.check_in_bounds(fp_origin));

    // 已存在合法站点 demo-LM1 (origin), 现在尝试在地图边界之外加站点.
    smap1::proto::Point3 max;
    max.set_x(1e9); max.set_y(1e9);
    if (map.pb().map().has_header() && map.pb().map().header().has_bounds()) {
        max = map.pb().map().header().bounds().max();
    }
    const double far_x = max.x() + 5.0;
    const double far_y = max.y() + 5.0;
    auto far_st = make_station("demo-far-out", far_x, far_y);
    if (auto r = map.add_station(std::move(far_st)); !r) {
        std::println("已按预期触发越界校验: {} (code={})",
                     r.error().message,
                     static_cast<int>(r.error().code));
    } else {
        std::println("意外: 未触发越界校验 (max=({:.3f},{:.3f}))", max.x(), max.y());
    }

    // 解除关联, 后续添加恢复无空间约束.
    map.clear_robot_model();
    if (auto r = map.add_station(make_station("demo-far-out2", far_x, far_y)); r) {
        std::println("解除 robot_model 后, 越界站点 {} 重新允许加入", (*r)->id());
        map.remove_station("demo-far-out2");
    }
}
