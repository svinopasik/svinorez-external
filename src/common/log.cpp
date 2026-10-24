#include "common/log.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <unistd.h>

namespace cs2esp {
namespace {

std::mutex g_mutex;
std::map<std::string, std::chrono::steady_clock::time_point> g_last_seen;

const char* level_tag(LogLevel level) {
    switch (level) {
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "INFO ";
}

// ANSI color only for interactive terminal; redirected output stays clean
bool colors_enabled() {
    static const bool enabled = ::isatty(STDOUT_FILENO) != 0;
    return enabled;
}

const char* level_color(LogLevel level) {
    if (!colors_enabled()) return "";
    switch (level) {
    case LogLevel::Info:  return "\x1b[32m";
    case LogLevel::Warn:  return "\x1b[33m";
    case LogLevel::Error: return "\x1b[31m";
    }
    return "";
}

const char* color_reset() {
    return colors_enabled() ? "\x1b[0m" : "";
}

void emit(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_mutex);

    const std::time_t now = std::time(nullptr);
    std::tm tm_local{};
    ::localtime_r(&now, &tm_local);

    char stamp[32];
    ::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm_local);

    std::FILE* out = (level == LogLevel::Error) ? stderr : stdout;
    std::fprintf(out, "[%s] [%s%s%s] %s\n", stamp, level_color(level),
                 level_tag(level), color_reset(), msg.c_str());
    if (level == LogLevel::Error) {
        std::fflush(stderr);
    }
}

} // namespace

void log_write(LogLevel level, const std::string& msg) {
    emit(level, msg);
}

void log_throttled(const std::string& key, int min_interval_sec,
                   LogLevel level, const std::string& msg) {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto now = std::chrono::steady_clock::now();
        const auto it = g_last_seen.find(key);
        if (it != g_last_seen.end()) {
            const auto since = std::chrono::duration_cast<std::chrono::seconds>(now - it->second).count();
            if (since < min_interval_sec) return;
            it->second = now;
        } else {
            g_last_seen.emplace(key, now);
        }
    }
    emit(level, msg);
}

} // namespace cs2esp
