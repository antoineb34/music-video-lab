#include "logger.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <regex>

namespace {

// Logger is a process-wide singleton; each test installs its own capturing
// sink and restores the default state on the way out so tests stay
// independent of run order.
struct LoggerFixture {
    std::mutex capture_mutex;
    std::vector<std::pair<mvlab::LogLevel, std::string>> captured;

    LoggerFixture()
    {
        mvlab::Logger::instance().set_sink([this](mvlab::LogLevel level, const std::string& message) {
            std::lock_guard<std::mutex> lock(capture_mutex);
            captured.emplace_back(level, message);
        });
    }

    ~LoggerFixture()
    {
        mvlab::Logger::instance().set_sink(nullptr);
        mvlab::Logger::instance().set_min_level(mvlab::LogLevel::warning);
    }
};

} // namespace

TEST_CASE("Logger: default level allows warning and error, filters info and debug", "[logger]")
{
    LoggerFixture fixture;
    mvlab::Logger::instance().set_min_level(mvlab::LogLevel::warning);

    mvlab::Logger::instance().error("err");
    mvlab::Logger::instance().warning("warn");
    mvlab::Logger::instance().info("info");
    mvlab::Logger::instance().debug("dbg");

    REQUIRE(fixture.captured.size() == 2);
    CHECK(fixture.captured[0].first == mvlab::LogLevel::error);
    CHECK(fixture.captured[1].first == mvlab::LogLevel::warning);
}

TEST_CASE("Logger: error level filters everything but errors", "[logger]")
{
    LoggerFixture fixture;
    mvlab::Logger::instance().set_min_level(mvlab::LogLevel::error);

    mvlab::Logger::instance().error("err");
    mvlab::Logger::instance().warning("warn");
    mvlab::Logger::instance().info("info");
    mvlab::Logger::instance().debug("dbg");

    REQUIRE(fixture.captured.size() == 1);
    CHECK(fixture.captured[0].first == mvlab::LogLevel::error);
}

TEST_CASE("Logger: info level (verbose) allows error, warning, info but not debug", "[logger]")
{
    LoggerFixture fixture;
    mvlab::Logger::instance().set_min_level(mvlab::LogLevel::info);

    mvlab::Logger::instance().error("err");
    mvlab::Logger::instance().warning("warn");
    mvlab::Logger::instance().info("info");
    mvlab::Logger::instance().debug("dbg");

    REQUIRE(fixture.captured.size() == 3);
    CHECK(fixture.captured[2].first == mvlab::LogLevel::info);
}

TEST_CASE("Logger: debug level allows everything", "[logger]")
{
    LoggerFixture fixture;
    mvlab::Logger::instance().set_min_level(mvlab::LogLevel::debug);

    mvlab::Logger::instance().error("err");
    mvlab::Logger::instance().warning("warn");
    mvlab::Logger::instance().info("info");
    mvlab::Logger::instance().debug("dbg");

    REQUIRE(fixture.captured.size() == 4);
    CHECK(fixture.captured[3].first == mvlab::LogLevel::debug);
}

TEST_CASE("Logger: quiet behavior (error-only) suppresses warning/info/debug", "[logger]")
{
    LoggerFixture fixture;
    mvlab::Logger::instance().set_min_level(mvlab::LogLevel::error);

    mvlab::Logger::instance().warning("should be suppressed");
    mvlab::Logger::instance().info("should be suppressed");
    mvlab::Logger::instance().debug("should be suppressed");

    CHECK(fixture.captured.empty());
}

TEST_CASE("Logger: capturing sink receives level and message", "[logger]")
{
    LoggerFixture fixture;
    mvlab::Logger::instance().set_min_level(mvlab::LogLevel::debug);

    mvlab::Logger::instance().info("hello world");

    REQUIRE(fixture.captured.size() == 1);
    CHECK(fixture.captured[0].first == mvlab::LogLevel::info);
    CHECK(fixture.captured[0].second == "hello world");
}

TEST_CASE("Logger: formatted line contains a timestamp, level, and message", "[logger]")
{
    std::string line = mvlab::format_log_line(mvlab::LogLevel::warning, "disk almost full");

    CHECK(line.find("WARNING") != std::string::npos);
    CHECK(line.find("disk almost full") != std::string::npos);

    std::regex timestamp_pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2})");
    CHECK(std::regex_search(line, timestamp_pattern));
}

TEST_CASE("Logger: to_string covers every LogLevel", "[logger]")
{
    CHECK(mvlab::to_string(mvlab::LogLevel::error) == "ERROR");
    CHECK(mvlab::to_string(mvlab::LogLevel::warning) == "WARNING");
    CHECK(mvlab::to_string(mvlab::LogLevel::info) == "INFO");
    CHECK(mvlab::to_string(mvlab::LogLevel::debug) == "DEBUG");
}

TEST_CASE("Logger: concurrent writes from multiple threads do not corrupt output", "[logger]")
{
    LoggerFixture fixture;
    mvlab::Logger::instance().set_min_level(mvlab::LogLevel::debug);

    constexpr int thread_count = 8;
    constexpr int messages_per_thread = 50;

    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([t]() {
            for (int i = 0; i < messages_per_thread; ++i) {
                mvlab::Logger::instance().info("thread " + std::to_string(t) + " msg " + std::to_string(i));
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }

    std::lock_guard<std::mutex> lock(fixture.capture_mutex);
    REQUIRE(fixture.captured.size() == static_cast<size_t>(thread_count * messages_per_thread));

    // Every captured message must be intact (no interleaved/corrupted
    // strings from concurrent sink invocations).
    std::regex pattern(R"(^thread \d+ msg \d+$)");
    for (const auto& [level, message] : fixture.captured) {
        CHECK(level == mvlab::LogLevel::info);
        CHECK(std::regex_match(message, pattern));
    }
}
