#pragma once

#include <smap1/error.hpp>
#include <smap1/ir.pb.h>

#include <expected>
#include <filesystem>

namespace smap1::codec {

/// 原始地图格式标识.  新增格式时在末尾追加枚举值.
enum class SourceFormat {
    /// RBK v3.4 单文件地图; path 指向 .smap 文件.
    Rbk34,
    /// RBK v3.5 文件夹地图; path 指向包含 info.json / *.smap / 2dft/ / 2dlh/ / 2dpc/ 的目录.
    Rbk35,
};

/// save 时的写出选项.
struct SaveOptions {
    /// 目标路径已存在且非空时是否覆盖.  默认 false, 此时返回 Error::Code::TargetExists.
    bool overwrite = false;
};

/// 从原始地图格式加载为 IR.
/// path 与 format 的语义见 SourceFormat 各枚举值的说明.
std::expected<proto::MapPackage, Error> load(const std::filesystem::path& path, SourceFormat format);

/// 将 IR 写回原始地图格式.
/// rbk34: path 为目标 .smap 文件路径; rbk35: path 为目标文件夹路径 (库负责创建).
/// 尽量利用 MapPackage.source 中的 SourceBundle / RawPayload / Property.legacy_value 做无损回写.
std::expected<void, Error> save(const proto::MapPackage& package, const std::filesystem::path& path, SourceFormat format, const SaveOptions& options = {});

}  // namespace smap1::codec
