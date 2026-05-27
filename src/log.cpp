#include <smap1/log.hpp>

#include "log_internal.hpp"

#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstdlib>
#include <memory>
#include <mutex>
#include <string_view>

namespace smap1 {

namespace {

spdlog::level::level_enum to_spdlog(LogLevel l) noexcept {
    switch (l) {
        case LogLevel::Trace:    return spdlog::level::trace;
        case LogLevel::Debug:    return spdlog::level::debug;
        case LogLevel::Info:     return spdlog::level::info;
        case LogLevel::Warn:     return spdlog::level::warn;
        case LogLevel::Error:    return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
        case LogLevel::Off:      return spdlog::level::off;
    }
    return spdlog::level::info;
}

LogLevel from_spdlog(spdlog::level::level_enum l) noexcept {
    switch (l) {
        case spdlog::level::trace:    return LogLevel::Trace;
        case spdlog::level::debug:    return LogLevel::Debug;
        case spdlog::level::info:     return LogLevel::Info;
        case spdlog::level::warn:     return LogLevel::Warn;
        case spdlog::level::err:      return LogLevel::Error;
        case spdlog::level::critical: return LogLevel::Critical;
        case spdlog::level::off:      return LogLevel::Off;
        case spdlog::level::n_levels: break;
    }
    return LogLevel::Info;
}

spdlog::level::level_enum initial_level_from_env() noexcept {
    const char* raw = std::getenv("SMAP1_LOG_LEVEL");
    if (raw == nullptr) return spdlog::level::info;
    const std::string_view s{raw};
    if (s == "trace")    return spdlog::level::trace;
    if (s == "debug")    return spdlog::level::debug;
    if (s == "info")     return spdlog::level::info;
    if (s == "warn")     return spdlog::level::warn;
    if (s == "error")    return spdlog::level::err;
    if (s == "critical") return spdlog::level::critical;
    if (s == "off")      return spdlog::level::off;
    return spdlog::level::info;
}

std::shared_ptr<spdlog::logger> make_logger() {
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    auto lg = std::make_shared<spdlog::logger>("smap1", std::move(sink));
    lg->set_level(initial_level_from_env());
    lg->set_pattern("[%H:%M:%S.%e] [%n] [%^%l%$] %v");
    return lg;
}

}  // namespace

namespace detail {

spdlog::logger& logger() {
    // Meyers singleton; thread-safe init on first call. Avoids touching the
    // spdlog global registry so multiple consumers can coexist without name
    // clashes.
    static const std::shared_ptr<spdlog::logger> instance = make_logger();
    return *instance;
}

}  // namespace detail

void set_log_level(LogLevel level) {
    detail::logger().set_level(to_spdlog(level));
}

LogLevel log_level() noexcept {
    return from_spdlog(detail::logger().level());
}

}  // namespace smap1
