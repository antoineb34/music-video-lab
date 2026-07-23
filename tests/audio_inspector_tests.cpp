#include "audio_inspector.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <sstream>

namespace fs = std::filesystem;

// Helper: Create a temporary WAV file and return its path
std::string create_temp_wav(double duration_seconds = 0.5)
{
    std::string temp_file = "/tmp/mvlab_test_audio_XXXXXX.wav";
    // Use a simple approach: create via ffmpeg
    std::ostringstream cmd;
    cmd << "ffmpeg -f lavfi -i anullsrc=r=48000:cl=stereo -t " << duration_seconds
        << " -q:a 9 " << temp_file.substr(0, temp_file.rfind(".wav")) + "_"
        << std::to_string(rand()) << ".wav -loglevel error 2>/dev/null";

    // Actually, let's use a simpler fixed path approach
    static int counter = 0;
    std::string path = "/tmp/mvlab_test_" + std::to_string(++counter) + ".wav";
    std::ostringstream cmd2;
    cmd2 << "ffmpeg -f lavfi -i anullsrc=r=48000:cl=stereo -t " << duration_seconds
         << " -q:a 9 '" << path << "' -loglevel error 2>/dev/null";

    int ret = std::system(cmd2.str().c_str());
    if (ret != 0) {
        return "";
    }

    // Verify file was created
    if (!fs::exists(path)) {
        return "";
    }

    return path;
}

// Helper: Clean up a temporary file
void cleanup_temp_file(const std::string& path)
{
    if (!path.empty() && fs::exists(path)) {
        fs::remove(path);
    }
}

TEST_CASE("mvlab::inspect_audio with valid WAV succeeds", "[inspector]")
{
    std::string temp_file = create_temp_wav(0.5);
    REQUIRE(!temp_file.empty());
    REQUIRE(fs::exists(temp_file));

    auto [result, error] = mvlab::inspect_audio(temp_file);

    CHECK(error.empty());
    CHECK(!result.codec.empty());
    CHECK(!result.sample_rate.empty());
    CHECK(!result.channels.empty());
    CHECK(!result.duration.empty());

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::inspect_audio contains all required fields", "[inspector]")
{
    std::string temp_file = create_temp_wav(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::inspect_audio(temp_file);

    REQUIRE(error.empty());

    // Check all 6 fields are populated
    CHECK_FALSE(result.codec.empty());
    CHECK_FALSE(result.sample_rate.empty());
    CHECK_FALSE(result.channels.empty());
    CHECK_FALSE(result.duration.empty());
    // Bitrate is either stream or format
    bool has_bitrate = !result.stream_bitrate.empty() || !result.format_bitrate.empty();
    CHECK(has_bitrate);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::inspect_audio nonexistent file returns error", "[inspector]")
{
    auto [result, error] = mvlab::inspect_audio("/tmp/mvlab_nonexistent_12345_xyz.wav");

    CHECK_FALSE(error.empty());
    bool has_error_msg = (error.find("not found") != std::string::npos ||
                          error.find("File not found") != std::string::npos);
    CHECK(has_error_msg);
}

TEST_CASE("mvlab::inspect_audio invalid file returns ffprobe error", "[inspector]")
{
    std::string temp_file = "/tmp/mvlab_test_invalid.txt";
    FILE* f = std::fopen(temp_file.c_str(), "w");
    std::fprintf(f, "This is not an audio file\n");
    std::fclose(f);

    auto [result, error] = mvlab::inspect_audio(temp_file);

    CHECK_FALSE(error.empty());
    bool has_error = (error.find("Error") != std::string::npos ||
                      error.find("Invalid") != std::string::npos);
    CHECK(has_error);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::inspect_audio path with spaces succeeds", "[inspector]")
{
    std::string base_file = create_temp_wav(0.5);
    REQUIRE(!base_file.empty());

    // Create a copy with spaces in the name
    std::string spaced_file = "/tmp/mvlab test audio with spaces.wav";
    std::string cmd = "cp '" + base_file + "' '" + spaced_file + "'";
    int ret = std::system(cmd.c_str());
    REQUIRE(ret == 0);
    REQUIRE(fs::exists(spaced_file));

    auto [result, error] = mvlab::inspect_audio(spaced_file);

    CHECK(error.empty());
    CHECK(!result.codec.empty());

    cleanup_temp_file(base_file);
    cleanup_temp_file(spaced_file);
}
