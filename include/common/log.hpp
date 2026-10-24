#pragma once

#include <string>
#include <mutex>
#include <map>

namespace cs2esp {

enum class LogLevel { Info, Warn, Error };

// Thread-safe logging to stdout/stderr. Error → stderr.
void log_write(LogLevel level, const std::string& msg);

// Same, but messages with the same key are no more frequent than once every min_interval_sec.
// For loops: "no game", "offsets not filled", and other repeating states don't flood the log.
void log_throttled(const std::string& key, int min_interval_sec,
                   LogLevel level, const std::string& msg);

inline void log_info(const std::string& msg) { log_write(LogLevel::Info, msg); }
inline void log_warn(const std::string& msg) { log_write(LogLevel::Warn, msg); }
inline void log_error(const std::string& msg) { log_write(LogLevel::Error, msg); }

} // namespace cs2esp
