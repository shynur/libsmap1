#pragma once

#include <spdlog/spdlog.h>

namespace smap1::detail {

/// 进程级 logger.  首次调用时按环境变量 SMAP1_LOG_LEVEL 初始化.
spdlog::logger& logger();

}  // namespace smap1::detail

#define SMAP1_LOG_TRACE(...)    ::smap1::detail::logger().trace(__VA_ARGS__)
#define SMAP1_LOG_DEBUG(...)    ::smap1::detail::logger().debug(__VA_ARGS__)
#define SMAP1_LOG_INFO(...)     ::smap1::detail::logger().info(__VA_ARGS__)
#define SMAP1_LOG_WARN(...)     ::smap1::detail::logger().warn(__VA_ARGS__)
#define SMAP1_LOG_ERROR(...)    ::smap1::detail::logger().error(__VA_ARGS__)
#define SMAP1_LOG_CRITICAL(...) ::smap1::detail::logger().critical(__VA_ARGS__)
