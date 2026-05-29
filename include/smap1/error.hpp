#pragma once

#include <string>

namespace smap1 {

/// libsmap1 公开 API 的错误类型.  可失败操作通过 std::expected<T, Error> 返回.
struct Error {
    /// 错误类别.  用于编程判定; 详细信息见 message.
    enum class Code {
        /// 文件或目录不存在.
        FileNotFound,
        /// 文件 I/O 失败 (读/写/权限/磁盘等).
        IoError,
        /// JSON 或 protobuf 解析失败.
        ParseFailed,
        /// save 目标路径已存在且非空, 但 SaveOptions::overwrite 为 false.
        TargetExists,
        /// 添加对象时其 ID 已被占用.
        IdConflict,
        /// 引用的对象 ID 不存在 (例如 Path 引用了不存在的 Station).
        IdNotFound,
        /// Path 的 CurveGeometry 首/尾控制点与起止站点的 pose 不一致.
        EndpointMismatch,
        /// 输入参数语义无效 (例如必填字段缺失).
        InvalidArgument,
        /// 待添加 / 待校验对象超出地图允许范围 (例如 robot footprint 越出地图 bounds).
        OutOfBounds,
    };

    /// 错误类别.
    Code code;
    /// 人类可读错误信息, 含具体上下文 (例如出错的 ID、文件路径).
    std::string message;
};

}  // namespace smap1
