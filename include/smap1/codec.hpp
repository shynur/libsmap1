#pragma once

#include <smap1/calibration.pb.h>
#include <smap1/error.hpp>
#include <smap1/ir.pb.h>
#include <smap1/robot_model.pb.h>

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

/// robot model 文件版本标识.  与 SourceFormat 平行, 但语义独立 —
/// rbk34 / rbk35 的 robot_model JSON 结构差别较大, 必须显式指定.
enum class RobotModelFormat {
    /// RBK v3.4 风格: 顶层 {"model": ..., "deviceTypes": [...]}, chassis 在
    /// deviceTypes[name=="chassis"].devices[0].deviceParams 里.
    Rbk34,
    /// RBK v3.5 风格: 顶层 {"groups": [...], "name": "robot"}, 关注的 Model
    /// 子组在 groups[key=="Model"].children[0].groups 中.
    Rbk35,
};

/// 标定文件 (robot.cp) 版本标识.  与 RobotModelFormat 平行 —
/// 两个版本的 .cp 都是 JSON, 但顶层结构完全不同, 必须显式指定.
enum class CalibrationFormat {
    /// RBK v3.4 风格: 顶层 {"deviceTypes": [...]}, 结构同 robot model 的
    /// deviceParams 树 (arrayParam / comboParam / 标量叶子).
    /// 在日志包中位于 models/robot.cp 与 models/bak/cp/*.robot.cp.
    Rbk34,
    /// RBK v3.5 风格: 顶层 {"model": {...}}, model 是扁平键值表, 键为
    /// "<DeviceType>.<DeviceName>.<param.path>" 点分隔路径, 值为标量;
    /// 形如 "<...>.<XxxCalib>": "Passed" 的项是标定状态而非参数.
    /// 在日志包中位于 private/calibrations/robot.cp.
    Rbk35,
};

/// save 时的写出选项.
struct SaveOptions {
    /// 目标路径已存在且非空时是否覆盖.  默认 false, 此时返回 Error::Code::TargetExists.
    bool overwrite = false;
};

/// 从原始地图格式加载为 IR.
/// path 与 format 的语义见 SourceFormat 各枚举值的说明.
std::expected<proto::MapPackage, Error> load(std::filesystem::path path, SourceFormat format);

/// 将 IR 写回原始地图格式.
/// rbk34: path 为目标 .smap 文件路径; rbk35: path 为目标文件夹路径 (库负责创建).
/// 尽量利用 MapPackage.source 中的 SourceBundle / RawPayload / Property.legacy_value 做无损回写.
std::expected<void, Error> save(const proto::MapPackage& package, std::filesystem::path path, SourceFormat format, const SaveOptions& options = {});

/// 从 robot model 文件加载为 IR.  robot model 是只读资产, 因此只提供 load,
/// 不提供 save.  解析失败 / 关键字段缺失会返回 ParseFailed 或 InvalidArgument.
std::expected<proto::RobotModel, Error> load_robot_model(std::filesystem::path path, RobotModelFormat format);

/// 从标定文件 (robot.cp) 加载为 IR.  标定文件是只读资产, 因此只提供 load,
/// 不提供 save.  文件不是合法 JSON 返回 ParseFailed; 顶层结构不符合
/// format 描述的形态 (rbk34 缺 deviceTypes / rbk35 缺 model) 返回 InvalidArgument.
std::expected<proto::Calibration, Error> load_calibration(std::filesystem::path path, CalibrationFormat format);

}  // namespace smap1::codec
