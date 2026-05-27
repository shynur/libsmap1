#pragma once

namespace smap1 {

/// libsmap1 内部日志的级别.  数值大小与 spdlog 一致 (trace 最低, off 最高).
enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off,
};

/// 设置库内部 logger 的最小输出级别.  线程安全.
///
/// 库初始化时会读取环境变量 SMAP1_LOG_LEVEL (取值 trace / debug / info /
/// warn / error / critical / off, 大小写敏感) 决定初始级别; 未设置时默认
/// Info.  调用本函数会覆盖该初始值.
void set_log_level(LogLevel level);

/// 返回当前 logger 的最小输出级别.
LogLevel log_level() noexcept;

}  // namespace smap1
