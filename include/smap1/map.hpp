#pragma once

#include <smap1/error.hpp>
#include <smap1/ir.pb.h>
#include <smap1/robot_model.pb.h>

#include <expected>
#include <string_view>
#include <vector>

namespace smap1 {

/// 地图 wrap 层.  拥有一个 proto::MapPackage, 提供站点 / 路径的 CRUD 便利方法.
///
/// 设计约定:
/// - 不持索引缓存, 所有方法基于当前底层 proto 内容即时计算.
/// - 通过 pb() 暴露底层 proto, 允许调用方直接读写所有字段; 调用方修改 proto 时
///   wrap 层不会感知, 但因为没有缓存, 一致性不会被破坏.
/// - 返回的 proto::Station* / proto::Path* 在底层 RepeatedPtrField 发生重分配时
///   (例如 add_* / remove_* / 调用方直接 add_stations) 可能失效.  调用方需自行
///   注意指针的生命周期.
class Map {
public:
    /// 以 package 进行构造; 调用方可传入左值 (复制) 或右值 (move).  内部将其 move
    /// 进成员.  不做任何校验; 如需检查内部一致性, 在构造后另行调用 validate.
    ///
    /// @note 常规流程是使用 codec 解析得到的 proto::MapPackage 进行初始化
    ///       (例如 smap1::codec::load 的返回值); 默认构造的空 proto 对象也可
    ///       接受, 但缺少元数据字段, 一般仅用于从零构建地图的场景.
    explicit Map(proto::MapPackage package) noexcept;

    Map(const Map&) = delete;
    Map& operator=(const Map&) = delete;
    Map(Map&&) noexcept = default;
    Map& operator=(Map&&) noexcept = default;
    ~Map() = default;

    /// 取底层 proto 的 const 引用.
    const proto::MapPackage& pb() const& noexcept;
    /// 取底层 proto 的可变引用.  调用方可直接修改任意字段.
    proto::MapPackage& pb() & noexcept;
    /// 移出底层 proto.  调用后此 Map 进入 moved-from 状态, 不应再使用.
    proto::MapPackage into_pb() && noexcept;

    // ---------------- Station ----------------

    /// 站点总数.
    int station_count() const noexcept;

    /// 按 ID 查找站点; 找不到返回 nullptr.
    const proto::Station* find_station(std::string_view id) const noexcept;
    /// 按 ID 查找站点 (可变); 找不到返回 nullptr.
    proto::Station* find_station(std::string_view id) noexcept;

    /// 添加站点.  校验:
    ///   - station.id 非空.
    ///   - station.id 在当前所有站点中唯一.
    /// 成功时将 station move 进底层 proto 并返回其指针; 失败时返回 Error.
    std::expected<proto::Station*, Error> add_station(proto::Station station);

    /// 按 ID 删除站点, 并级联删除所有引用此站点的 Path.  返回是否真的删除了一个站点.
    bool remove_station(std::string_view id);

    // ---------------- Path ----------------

    /// 路径总数.
    int path_count() const noexcept;

    /// 按 ID 查找路径; 找不到返回 nullptr.
    const proto::Path* find_path(std::string_view id) const noexcept;
    /// 按 ID 查找路径 (可变); 找不到返回 nullptr.
    proto::Path* find_path(std::string_view id) noexcept;

    /// 列出从 start_station_id 到 end_station_id 的所有路径.
    std::vector<const proto::Path*> find_paths_between(std::string_view start_station_id, std::string_view end_station_id) const;
    /// 同上的可变版本.
    std::vector<proto::Path*> find_paths_between(std::string_view start_station_id, std::string_view end_station_id);

    /// 添加路径.  校验:
    ///   - path.id 非空且在当前所有路径中唯一.
    ///   - path.start_station_id / end_station_id 指向已存在的站点.
    ///   - path.geometry.control_points 非空, 且首/尾控制点等于起止站点的 pose.
    /// 成功时将 path move 进底层 proto 并返回其指针; 失败时返回 Error.
    std::expected<proto::Path*, Error> add_path(proto::Path path);

    /// 按 ID 删除路径.  返回是否真的删除了一个路径.
    bool remove_path(std::string_view id);

    // ---------------- Robot model ----------------

    /// 设置当前地图所搭配的 robot model.  调用方按值传入 (左值复制 / 右值 move),
    /// Map 内部按值持有一份, 不引用原对象.
    ///
    /// 语义:
    /// - 设置后 add_station 会顺带做 footprint 越界校验 (前提: 地图 header 中
    ///   已写入 bounds; 否则不做空间检查).
    /// - 不会自动重新校验已有站点.  如需复检, 调用 footprint_at + check_in_bounds.
    void set_robot_model(proto::RobotModel model);

    /// 解除当前 robot model 关联.  之后 robot_model() 返回 nullptr.
    void clear_robot_model() noexcept;

    /// 当前关联的 robot model; 未设置时返回 nullptr.
    const proto::RobotModel* robot_model() const noexcept;

    /// 计算给定 pose 处的 robot footprint, 顶点坐标已落到 map 坐标系.
    ///
    /// 步骤: shape 顶点 (chassis 局部坐标系) -> 应用 shape_to_chassis -> 应用 pose.
    /// 矩形按 [-tail, +head] x [-width/2, +width/2] 展开; 圆形按
    /// `samples` 个均匀点近似 (默认 32, 调用方可加大以更精细).
    ///
    /// robot_model 未设置时返回空 Polygon.
    proto::Polygon footprint_at(const proto::Pose2D& pose, int samples = 32) const;

    /// 检查给定 footprint 是否完整落在地图 header 的 bounds 之内.
    ///
    /// 仅当 header.bounds 同时设置了 min 与 max (且 min < max) 时才会做检查;
    /// 否则视为无 bounds, 直接通过.
    bool check_in_bounds(const proto::Polygon& footprint) const noexcept;

private:
    proto::MapPackage package_;
    // 内嵌一份 RobotModel; has_robot_model_ 表示是否被设置过, 用于区分 "未设置"
    // 和 "设置了一个全字段都是默认值的 model".
    proto::RobotModel robot_model_;
    bool has_robot_model_ = false;
};

}  // namespace smap1
