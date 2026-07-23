#include "error.hpp"
#include <catch2/catch_test_macros.hpp>
#include <array>

TEST_CASE("Error: every ErrorCode has a stable non-empty string representation", "[error]")
{
    constexpr std::array codes = {
        mvlab::ErrorCode::none,
        mvlab::ErrorCode::invalid_argument,
        mvlab::ErrorCode::file_not_found,
        mvlab::ErrorCode::permission_denied,
        mvlab::ErrorCode::invalid_media,
        mvlab::ErrorCode::external_tool_unavailable,
        mvlab::ErrorCode::external_tool_failed,
        mvlab::ErrorCode::malformed_external_output,
        mvlab::ErrorCode::malformed_project,
        mvlab::ErrorCode::unsupported_format,
        mvlab::ErrorCode::unsupported_schema,
        mvlab::ErrorCode::filesystem_error,
        mvlab::ErrorCode::serialization_error,
        mvlab::ErrorCode::internal_error,
    };

    for (auto code : codes) {
        CHECK_FALSE(mvlab::to_string(code).empty());
    }
}

TEST_CASE("Error: to_string is stable across repeated calls", "[error]")
{
    CHECK(mvlab::to_string(mvlab::ErrorCode::file_not_found) == mvlab::to_string(mvlab::ErrorCode::file_not_found));
    CHECK(mvlab::to_string(mvlab::ErrorCode::file_not_found) == "file_not_found");
}

TEST_CASE("Error: concise message works without details", "[error]")
{
    mvlab::Error err{mvlab::ErrorCode::invalid_argument, "bad input", std::nullopt};
    CHECK(err.message == "bad input");
    CHECK_FALSE(err.details.has_value());
}

TEST_CASE("Error: details are optional and preserved when present", "[error]")
{
    mvlab::Error err{mvlab::ErrorCode::external_tool_failed, "ffprobe failed", std::string("stderr: bad input")};
    REQUIRE(err.details.has_value());
    CHECK(err.details.value() == "stderr: bad input");
}

TEST_CASE("Error: exit_code_for covers every ErrorCode with the documented mapping", "[error]")
{
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::none) == 0);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::invalid_argument) == 2);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::file_not_found) == 3);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::permission_denied) == 3);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::filesystem_error) == 3);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::external_tool_unavailable) == 4);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::external_tool_failed) == 4);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::invalid_media) == 5);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::malformed_external_output) == 5);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::malformed_project) == 5);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::unsupported_format) == 5);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::unsupported_schema) == 5);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::serialization_error) == 5);
    CHECK(mvlab::exit_code_for(mvlab::ErrorCode::internal_error) == 10);
}
