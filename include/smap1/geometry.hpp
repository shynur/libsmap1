#pragma once

#include <smap1/ir.pb.h>

#include <vector>

namespace smap1::geometry {

/// 构造 kind = LINE_SEGMENT 的 CurveGeometry, control_points = [start, end].
proto::CurveGeometry make_line_segment(const proto::Point3& start, const proto::Point3& end);

/// 构造 kind = BEZIER 的 CurveGeometry, degree = control_points.size() - 1.
/// 退化情况 (例如 5 阶 Bezier 中间 4 个控制点两两重合) 直接用重复点表示.
/// 不做语义校验; control_points 为空或单点时返回的 CurveGeometry 无效.
proto::CurveGeometry make_bezier(std::vector<proto::Point3> control_points);

}  // namespace smap1::geometry
