#pragma once

#include <smap1/error.hpp>
#include <smap1/ir.pb.h>

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

private:
    proto::MapPackage package_;
};

}  // namespace smap1
