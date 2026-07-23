#include "logger.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace mvlab {

std::string to_string(LogLevel level)
{
    switch (level) {
        case LogLevel::error:   return "ERROR";
        case LogLevel::warning: return "WARNING";
        case LogLevel::info:    return "INFO";
        case LogLevel::debug:   return "DEBUG";
    }
    return "UNKNOWN";
}

namespace {

std::string current_timestamp()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    localtime_r(&now_time, &tm_buf);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

} // namespace

std::string format_log_line(LogLevel level, const std::string& message)
{
    return "[" + current_timestamp() + "] [" + to_string(level) + "] " + message;
}

namespace {

void default_sink(LogLevel level, const std::string& message)
{
    std::cerr << format_log_line(level, message) << "\n";
}

} // namespace

Logger::Logger()
    : min_level_(LogLevel::warning)
    , sink_(default_sink)
{
}

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

void Logger::set_min_level(LogLevel level)
{
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

LogLevel Logger::min_level() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return min_level_;
}

void Logger::set_sink(LogSink sink)
{
    std::lock_guard<std::mutex> lock(mutex_);
    sink_ = sink ? std::move(sink) : LogSink(default_sink);
}

void Logger::log(LogLevel level, const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // Lower enum value == higher severity (error=0 .. debug=3); a message
    // is emitted when it is at least as severe as the configured floor.
    if (static_cast<int>(level) > static_cast<int>(min_level_)) {
        return;
    }
    if (sink_) {
        sink_(level, message);
    }
}

void Logger::error(const std::string& message) { log(LogLevel::error, message); }
void Logger::warning(const std::string& message) { log(LogLevel::warning, message); }
void Logger::info(const std::string& message) { log(LogLevel::info, message); }
void Logger::debug(const std::string& message) { log(LogLevel::debug, message); }

} // namespace mvlab
