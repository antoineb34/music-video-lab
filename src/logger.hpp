#ifndef MVLAB_LOGGER_HPP
#define MVLAB_LOGGER_HPP

#include <string>
#include <functional>
#include <mutex>

namespace mvlab {

enum class LogLevel {
    error,
    warning,
    info,
    debug
};

std::string to_string(LogLevel level);

// Formats a single log line the way the default stderr sink does:
// "[<timestamp>] [<LEVEL>] <message>". Exposed so tests (and any future
// custom sink) can reuse the same timestamped format without capturing
// the real stderr stream.
std::string format_log_line(LogLevel level, const std::string& message);

using LogSink = std::function<void(LogLevel level, const std::string& message)>;

// Process-wide logger. A CLI process has exactly one diagnostic stream
// (stderr by default), so a narrow singleton is used here instead of
// threading a logger instance through every function call; the interface
// is kept small (level filtering + a swappable sink) to keep that choice
// low-risk. All state is mutex-guarded and the sink is invoked while the
// lock is held, so concurrent log calls are serialized and a custom sink
// (e.g. a test's capturing sink) never races with itself.
class Logger {
public:
    static Logger& instance();

    void set_min_level(LogLevel level);
    LogLevel min_level() const;

    // Installs a custom sink (e.g. for tests). Pass nullptr to restore
    // the default stderr sink.
    void set_sink(LogSink sink);

    void log(LogLevel level, const std::string& message);

    void error(const std::string& message);
    void warning(const std::string& message);
    void info(const std::string& message);
    void debug(const std::string& message);

private:
    Logger();

    mutable std::mutex mutex_;
    LogLevel min_level_;
    LogSink sink_;
};

} // namespace mvlab

#endif // MVLAB_LOGGER_HPP
