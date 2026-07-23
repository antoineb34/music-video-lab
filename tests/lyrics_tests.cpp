#include "lyrics.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {

// Helper: Create a temporary text file with given contents
static std::string create_temp_lyrics_file(const std::string& extension, const std::string& contents)
{
    static int counter = 0;
    std::string path = "/tmp/mvlab_lyrics_" + std::to_string(++counter) + extension;

    std::ofstream file(path);
    if (!file) return "";
    file << contents;
    file.close();

    return path;
}

// Helper: Clean up a temporary file
static void cleanup_temp_file(const std::string& path)
{
    if (!path.empty() && fs::exists(path)) {
        fs::remove(path);
    }
}

} // anonymous namespace

// ===== Plain Text Parsing Tests =====

TEST_CASE("parse_plain_text_lyrics: single line", "[lyrics][plain_text]")
{
    std::string contents = "Hello world";
    auto result = mvlab::parse_plain_text_lyrics(contents);

    REQUIRE(result.has_value());
    const auto& lyrics = result.value();
    CHECK(lyrics.format == mvlab::LyricsFormat::plain_text);
    REQUIRE(lyrics.lines.size() == 1);
    CHECK(lyrics.lines[0].text == "Hello world");
    CHECK(!lyrics.lines[0].start_time.has_value());
}

TEST_CASE("parse_plain_text_lyrics: multiple lines", "[lyrics][plain_text]")
{
    std::string contents = "Line one\nLine two\nLine three";
    auto result = mvlab::parse_plain_text_lyrics(contents);

    REQUIRE(result.has_value());
    const auto& lyrics = result.value();
    REQUIRE(lyrics.lines.size() == 3);
    CHECK(lyrics.lines[0].text == "Line one");
    CHECK(lyrics.lines[1].text == "Line two");
    CHECK(lyrics.lines[2].text == "Line three");
}

TEST_CASE("parse_plain_text_lyrics: Unix line endings", "[lyrics][plain_text]")
{
    std::string contents = "First\nSecond\nThird";
    auto result = mvlab::parse_plain_text_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 3);
    CHECK(result.value().lines[0].text == "First");
}

TEST_CASE("parse_plain_text_lyrics: Windows line endings", "[lyrics][plain_text]")
{
    std::string contents = "First\r\nSecond\r\nThird";
    auto result = mvlab::parse_plain_text_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 3);
    CHECK(result.value().lines[0].text == "First");
    CHECK(result.value().lines[1].text == "Second");
}

TEST_CASE("parse_plain_text_lyrics: blank lines skipped", "[lyrics][plain_text]")
{
    std::string contents = "Line one\n\nLine two\n\n\nLine three";
    auto result = mvlab::parse_plain_text_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 3);
    CHECK(result.value().lines[0].text == "Line one");
    CHECK(result.value().lines[1].text == "Line two");
    CHECK(result.value().lines[2].text == "Line three");
}

TEST_CASE("parse_plain_text_lyrics: whitespace trimmed", "[lyrics][plain_text]")
{
    std::string contents = "  Line one  \n\t  Line two  \t\n   Line three   ";
    auto result = mvlab::parse_plain_text_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 3);
    CHECK(result.value().lines[0].text == "Line one");
    CHECK(result.value().lines[1].text == "Line two");
    CHECK(result.value().lines[2].text == "Line three");
}

TEST_CASE("parse_plain_text_lyrics: order preserved", "[lyrics][plain_text]")
{
    std::string contents = "First\nSecond\nThird\nFourth\nFifth";
    auto result = mvlab::parse_plain_text_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 5);
    CHECK(result.value().lines[0].text == "First");
    CHECK(result.value().lines[4].text == "Fifth");
}

TEST_CASE("parse_plain_text_lyrics: empty input rejected", "[lyrics][plain_text]")
{
    std::string contents = "";
    auto result = mvlab::parse_plain_text_lyrics(contents);

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::malformed_external_output);
}

TEST_CASE("parse_plain_text_lyrics: whitespace-only input rejected", "[lyrics][plain_text]")
{
    std::string contents = "   \n\t\n   \n";
    auto result = mvlab::parse_plain_text_lyrics(contents);

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::malformed_external_output);
}

// ===== LRC Timestamp Tests =====

TEST_CASE("parse_lrc_lyrics: simple timestamp [mm:ss]", "[lyrics][lrc][timestamp]")
{
    std::string contents = "[00:10]First line\n[00:20]Second line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    const auto& lyrics = result.value();
    REQUIRE(lyrics.lines.size() == 2);

    CHECK(lyrics.lines[0].text == "First line");
    REQUIRE(lyrics.lines[0].start_time.has_value());
    CHECK(lyrics.lines[0].start_time.value() == 10000);

    CHECK(lyrics.lines[1].text == "Second line");
    REQUIRE(lyrics.lines[1].start_time.has_value());
    CHECK(lyrics.lines[1].start_time.value() == 20000);
}

TEST_CASE("parse_lrc_lyrics: centisecond precision [mm:ss.xx]", "[lyrics][lrc][timestamp]")
{
    std::string contents = "[01:23.45]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 1);

    mvlab::LyricTime expected = 1 * 60000 + 23 * 1000 + 450;
    CHECK(result.value().lines[0].start_time.value() == expected);
}

TEST_CASE("parse_lrc_lyrics: millisecond precision [mm:ss.xxx]", "[lyrics][lrc][timestamp]")
{
    std::string contents = "[01:23.456]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 1);

    mvlab::LyricTime expected = 1 * 60000 + 23 * 1000 + 456;
    CHECK(result.value().lines[0].start_time.value() == expected);
}

TEST_CASE("parse_lrc_lyrics: minutes over 99", "[lyrics][lrc][timestamp]")
{
    std::string contents = "[123:45]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 1);

    mvlab::LyricTime expected = 123 * 60000 + 45 * 1000;
    CHECK(result.value().lines[0].start_time.value() == expected);
}

TEST_CASE("parse_lrc_lyrics: second value 59 accepted", "[lyrics][lrc][timestamp]")
{
    std::string contents = "[00:59]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 1);
    CHECK(result.value().lines[0].start_time.value() == 59000);
}

TEST_CASE("parse_lrc_lyrics: second value 60 rejected", "[lyrics][lrc][timestamp]")
{
    std::string contents = "[00:60]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::malformed_external_output);
}

TEST_CASE("parse_lrc_lyrics: multiple timestamps on one line", "[lyrics][lrc][timestamp]")
{
    std::string contents = "[00:10][00:20]Repeated line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 2);

    CHECK(result.value().lines[0].text == "Repeated line");
    CHECK(result.value().lines[0].start_time.value() == 10000);

    CHECK(result.value().lines[1].text == "Repeated line");
    CHECK(result.value().lines[1].start_time.value() == 20000);
}

TEST_CASE("parse_lrc_lyrics: source order preserved", "[lyrics][lrc]")
{
    std::string contents = "[00:10]A\n[00:05]B\n[00:20]C";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 3);
    CHECK(result.value().lines[0].text == "A");
    CHECK(result.value().lines[0].start_time.value() == 10000);
    CHECK(result.value().lines[1].text == "B");
    CHECK(result.value().lines[1].start_time.value() == 5000);
}

TEST_CASE("parse_lrc_lyrics: duplicate timestamps preserved", "[lyrics][lrc]")
{
    std::string contents = "[00:10]Line A\n[00:10]Line B";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 2);
    CHECK(result.value().lines[0].start_time.value() == 10000);
    CHECK(result.value().lines[1].start_time.value() == 10000);
}

TEST_CASE("parse_lrc_lyrics: empty lyric text rejected", "[lyrics][lrc]")
{
    std::string contents = "[00:10]";
    auto result = mvlab::parse_lrc_lyrics(contents);

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::malformed_external_output);
}

TEST_CASE("parse_lrc_lyrics: malformed bracket rejected", "[lyrics][lrc]")
{
    std::string contents = "[00:10Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    CHECK(!result.has_value());
}

TEST_CASE("parse_lrc_lyrics: malformed minute rejected", "[lyrics][lrc]")
{
    std::string contents = "[AB:10]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    CHECK(!result.has_value());
}

TEST_CASE("parse_lrc_lyrics: malformed second rejected", "[lyrics][lrc]")
{
    std::string contents = "[00:AB]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    CHECK(!result.has_value());
}

TEST_CASE("parse_lrc_lyrics: malformed fraction rejected", "[lyrics][lrc]")
{
    std::string contents = "[00:10.XY]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    CHECK(!result.has_value());
}

TEST_CASE("parse_lrc_lyrics: unsupported fractional length rejected", "[lyrics][lrc]")
{
    std::string contents = "[00:10.1]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    CHECK(!result.has_value());
}

TEST_CASE("parse_lrc_lyrics: no usable lines rejected", "[lyrics][lrc]")
{
    std::string contents = "[ar:Artist]\n[ti:Title]";
    auto result = mvlab::parse_lrc_lyrics(contents);

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::malformed_external_output);
}

// ===== LRC Metadata Tests =====

TEST_CASE("parse_lrc_lyrics: recognized artist tag ignored", "[lyrics][lrc][metadata]")
{
    std::string contents = "[ar:The Beatles]\n[00:10]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 1);
    CHECK(result.value().lines[0].text == "Lyric line");
}

TEST_CASE("parse_lrc_lyrics: recognized title tag ignored", "[lyrics][lrc][metadata]")
{
    std::string contents = "[ti:Song Title]\n[00:10]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 1);
}

TEST_CASE("parse_lrc_lyrics: recognized album tag ignored", "[lyrics][lrc][metadata]")
{
    std::string contents = "[al:Album Name]\n[00:10]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 1);
}

TEST_CASE("parse_lrc_lyrics: recognized creator tag ignored", "[lyrics][lrc][metadata]")
{
    std::string contents = "[by:Creator Name]\n[00:10]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 1);
}

TEST_CASE("parse_lrc_lyrics: recognized offset tag ignored", "[lyrics][lrc][metadata]")
{
    std::string contents = "[offset:123]\n[00:10]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 1);
}

TEST_CASE("parse_lrc_lyrics: metadata mixed with lyrics", "[lyrics][lrc][metadata]")
{
    std::string contents = "[ar:Artist]\n[00:10]First\n[ti:Title]\n[00:20]Second\n[al:Album]\n[00:30]Third";
    auto result = mvlab::parse_lrc_lyrics(contents);

    REQUIRE(result.has_value());
    REQUIRE(result.value().lines.size() == 3);
    CHECK(result.value().lines[0].text == "First");
    CHECK(result.value().lines[1].text == "Second");
    CHECK(result.value().lines[2].text == "Third");
}

TEST_CASE("parse_lrc_lyrics: unknown metadata tag rejected", "[lyrics][lrc][metadata]")
{
    std::string contents = "[xx:Unknown]\n[00:10]Lyric line";
    auto result = mvlab::parse_lrc_lyrics(contents);

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::malformed_external_output);
}

// ===== Format Detection Tests =====

TEST_CASE("detect_lyrics_format: .txt extension", "[lyrics][format]")
{
    std::string temp_file = create_temp_lyrics_file(".txt", "test content");
    REQUIRE(!temp_file.empty());

    auto result = mvlab::detect_lyrics_format(temp_file);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::LyricsFormat::plain_text);

    cleanup_temp_file(temp_file);
}

TEST_CASE("detect_lyrics_format: .lrc extension", "[lyrics][format]")
{
    std::string temp_file = create_temp_lyrics_file(".lrc", "[00:10]test");
    REQUIRE(!temp_file.empty());

    auto result = mvlab::detect_lyrics_format(temp_file);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::LyricsFormat::lrc);

    cleanup_temp_file(temp_file);
}

TEST_CASE("detect_lyrics_format: uppercase extension", "[lyrics][format]")
{
    std::string temp_file = create_temp_lyrics_file(".TXT", "test");
    REQUIRE(!temp_file.empty());

    auto result = mvlab::detect_lyrics_format(temp_file);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::LyricsFormat::plain_text);

    cleanup_temp_file(temp_file);
}

TEST_CASE("detect_lyrics_format: mixed-case extension", "[lyrics][format]")
{
    std::string temp_file = create_temp_lyrics_file(".LrC", "[00:10]test");
    REQUIRE(!temp_file.empty());

    auto result = mvlab::detect_lyrics_format(temp_file);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::LyricsFormat::lrc);

    cleanup_temp_file(temp_file);
}

TEST_CASE("detect_lyrics_format: unsupported extension", "[lyrics][format]")
{
    std::string temp_file = create_temp_lyrics_file(".md", "test");
    REQUIRE(!temp_file.empty());

    auto result = mvlab::detect_lyrics_format(temp_file);

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::unsupported_format);

    cleanup_temp_file(temp_file);
}

TEST_CASE("detect_lyrics_format: missing file", "[lyrics][format]")
{
    auto result = mvlab::detect_lyrics_format("/nonexistent/path/file.txt");

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::file_not_found);
}

TEST_CASE("detect_lyrics_format: directory passed as file", "[lyrics][format]")
{
    auto result = mvlab::detect_lyrics_format("/tmp");

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
}

// ===== File Parsing Tests =====

TEST_CASE("parse_lyrics_file: .txt file parsing", "[lyrics][file]")
{
    std::string temp_file = create_temp_lyrics_file(".txt", "First line\nSecond line");
    REQUIRE(!temp_file.empty());

    auto result = mvlab::parse_lyrics_file(temp_file);

    REQUIRE(result.has_value());
    const auto& lyrics = result.value();
    CHECK(lyrics.format == mvlab::LyricsFormat::plain_text);
    REQUIRE(lyrics.lines.size() == 2);
    CHECK(lyrics.lines[0].text == "First line");
    CHECK(lyrics.lines[1].text == "Second line");

    cleanup_temp_file(temp_file);
}

TEST_CASE("parse_lyrics_file: .lrc file parsing", "[lyrics][file]")
{
    std::string temp_file = create_temp_lyrics_file(".lrc", "[00:10]First\n[00:20]Second");
    REQUIRE(!temp_file.empty());

    auto result = mvlab::parse_lyrics_file(temp_file);

    REQUIRE(result.has_value());
    const auto& lyrics = result.value();
    CHECK(lyrics.format == mvlab::LyricsFormat::lrc);
    REQUIRE(lyrics.lines.size() == 2);
    CHECK(lyrics.lines[0].text == "First");
    CHECK(lyrics.lines[0].start_time.value() == 10000);

    cleanup_temp_file(temp_file);
}

TEST_CASE("parse_lyrics_file: missing file returns error", "[lyrics][file]")
{
    auto result = mvlab::parse_lyrics_file("/nonexistent/lyrics.txt");

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::file_not_found);
}

TEST_CASE("parse_lyrics_file: empty plain text file rejected", "[lyrics][file]")
{
    std::string temp_file = create_temp_lyrics_file(".txt", "");
    REQUIRE(!temp_file.empty());

    auto result = mvlab::parse_lyrics_file(temp_file);

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::malformed_external_output);

    cleanup_temp_file(temp_file);
}

TEST_CASE("parse_lyrics_file: empty LRC file rejected", "[lyrics][file]")
{
    std::string temp_file = create_temp_lyrics_file(".lrc", "");
    REQUIRE(!temp_file.empty());

    auto result = mvlab::parse_lyrics_file(temp_file);

    CHECK(!result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::malformed_external_output);

    cleanup_temp_file(temp_file);
}
