#pragma once

#include <smap1/ir.pb.h>

#include <string>
#include <vector>

namespace smap1 {

class Map;

/// validate 报告的一条问题.
struct Issue {
    /// 问题类别.
    enum class Kind {
        /// 同类对象内出现重复 ID.
        DuplicateId,
        /// Path 引用了不存在的站点.
        DanglingStationRef,
        /// Path 的 CurveGeometry 首/尾控制点与起止站点的 pose 不一致.
        EndpointMismatch,
        /// CurveGeometry 的 degree 与 control_points 数量不匹配.
        ControlPointCountMismatch,
        /// 必填字段缺失或语义无效.
        InvalidField,
    };

    /// 问题类别.
    Kind kind;
    /// 出问题的对象类型名 (例如 "Station", "Path"); 用于定位.
    std::string object_type;
    /// 出问题的对象 ID; 若不适用则为空.
    std::string object_id;
    /// 人类可读描述.
    std::string message;
};

/// 检查 package 的内部一致性, 返回所有问题.  空 vector 表示无问题.
/// 检查项见 docs/api.md 的 Validation 章节.
std::vector<Issue> validate(const proto::MapPackage& package);

/// 等价于 validate(map.pb()).
std::vector<Issue> validate(const Map& map);

}  // namespace smap1
