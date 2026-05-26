#include <smap1/geometry.hpp>

#include <cstdint>
#include <utility>

namespace smap1::geometry {

proto::CurveGeometry make_line_segment(const proto::Point3& start, const proto::Point3& end) {
    proto::CurveGeometry g;
    g.set_kind(proto::CURVE_KIND_LINE_SEGMENT);
    g.set_degree(1);
    *g.add_control_points() = start;
    *g.add_control_points() = end;
    return g;
}

proto::CurveGeometry make_bezier(std::vector<proto::Point3> control_points) {
    proto::CurveGeometry g;
    g.set_kind(proto::CURVE_KIND_BEZIER);
    g.set_degree(
        control_points.empty()
            ? 0u
            : static_cast<std::uint32_t>(control_points.size() - 1)
    );
    auto* dst = g.mutable_control_points();
    dst->Reserve(static_cast<int>(control_points.size()));
    for (auto& cp : control_points) {
        *dst->Add() = std::move(cp);
    }
    return g;
}

}  // namespace smap1::geometry
