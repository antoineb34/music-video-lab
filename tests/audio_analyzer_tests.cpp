#include "audio_analyzer.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <cstdlib>
#include <string>
#include <sstream>

namespace fs = std::filesystem;

// Helper: Create a temporary WAV file and return its path
static std::string create_temp_wav_analyzer(double duration_seconds = 0.5)
{
    static int counter = 0;
    std::string path = "/tmp/mvlab_analyze_" + std::to_string(++counter) + ".wav";

    std::ostringstream cmd;
    cmd << "ffmpeg -f lavfi -i anullsrc=r=48000:cl=stereo -t " << duration_seconds
        << " -q:a 9 '" << path << "' -loglevel error 2>/dev/null";

    int ret = std::system(cmd.str().c_str());
    if (ret != 0) {
        return "";
    }

    if (!fs::exists(path)) {
        return "";
    }

    return path;
}

// Helper: Create temporary mono WAV
static std::string create_temp_mono_wav(double duration_seconds = 0.5)
{
    static int counter = 0;
    std::string path = "/tmp/mvlab_analyze_mono_" + std::to_string(++counter) + ".wav";

    std::ostringstream cmd;
    cmd << "ffmpeg -f lavfi -i anullsrc=r=48000:cl=mono -t " << duration_seconds
        << " -q:a 9 '" << path << "' -loglevel error 2>/dev/null";

    int ret = std::system(cmd.str().c_str());
    if (ret != 0) {
        return "";
    }

    if (!fs::exists(path)) {
        return "";
    }

    return path;
}


// Helper: Clean up a temporary file
static void cleanup_temp_file(const std::string& path)
{
    if (!path.empty() && fs::exists(path)) {
        fs::remove(path);
    }
}

TEST_CASE("mvlab::analyze_audio with valid stereo WAV succeeds", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());
    REQUIRE(fs::exists(temp_file));

    auto [result, error] = mvlab::analyze_audio(temp_file);

    CHECK(error.empty());
    CHECK(result.total_frames > 0);
    CHECK(result.channels > 0);
    CHECK(result.sample_rate > 0);
    CHECK(result.waveform_envelope.size() == 100);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio with valid mono WAV succeeds", "[analyzer]")
{
    std::string temp_file = create_temp_mono_wav(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::analyze_audio(temp_file);

    CHECK(error.empty());
    CHECK(result.channels == 1);
    CHECK(result.total_frames > 0);
    CHECK(result.sample_rate > 0);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio envelope point count matches --points", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto [result_50, error_50] = mvlab::analyze_audio(temp_file, 50);
    CHECK(error_50.empty());
    CHECK(result_50.waveform_envelope.size() == 50);

    auto [result_200, error_200] = mvlab::analyze_audio(temp_file, 200);
    CHECK(error_200.empty());
    CHECK(result_200.waveform_envelope.size() == 200);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio peak amplitude within range", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::analyze_audio(temp_file);

    CHECK(error.empty());
    CHECK(result.peak_amplitude >= 0.0);
    CHECK(result.peak_amplitude <= 1.0);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio RMS amplitude within range", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::analyze_audio(temp_file);

    CHECK(error.empty());
    CHECK(result.rms_amplitude >= 0.0);
    CHECK(result.rms_amplitude <= 1.0);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio silent audio returns near-zero peak and RMS", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::analyze_audio(temp_file);

    CHECK(error.empty());
    // Silence should have very low amplitude
    CHECK(result.peak_amplitude < 0.05);
    CHECK(result.rms_amplitude < 0.05);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio nonexistent file returns error", "[analyzer]")
{
    auto [result, error] = mvlab::analyze_audio("/tmp/mvlab_nonexistent_audio_xyz.wav");

    CHECK_FALSE(error.empty());
    bool has_error_msg = (error.find("not found") != std::string::npos ||
                          error.find("File not found") != std::string::npos);
    CHECK(has_error_msg);
}

TEST_CASE("mvlab::analyze_audio invalid file returns error", "[analyzer]")
{
    std::string temp_file = "/tmp/mvlab_analyze_invalid.txt";
    FILE* f = std::fopen(temp_file.c_str(), "w");
    std::fprintf(f, "This is not an audio file\n");
    std::fclose(f);

    auto [result, error] = mvlab::analyze_audio(temp_file);

    CHECK_FALSE(error.empty());

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio points=0 is rejected", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::analyze_audio(temp_file, 0);

    CHECK_FALSE(error.empty());
    bool has_error_msg = (error.find("greater than zero") != std::string::npos ||
                          error.find("Points must be") != std::string::npos);
    CHECK(has_error_msg);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio points=-1 is rejected", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::analyze_audio(temp_file, -1);

    CHECK_FALSE(error.empty());

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio path with spaces succeeds", "[analyzer]")
{
    std::string base_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!base_file.empty());

    std::string spaced_file = "/tmp/mvlab analyze audio with spaces.wav";
    std::string cmd = "cp '" + base_file + "' '" + spaced_file + "'";
    int ret = std::system(cmd.c_str());
    REQUIRE(ret == 0);
    REQUIRE(fs::exists(spaced_file));

    auto [result, error] = mvlab::analyze_audio(spaced_file);

    CHECK(error.empty());
    CHECK(result.total_frames > 0);

    cleanup_temp_file(base_file);
    cleanup_temp_file(spaced_file);
}

TEST_CASE("mvlab::analyze_audio envelope values within range", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::analyze_audio(temp_file);

    CHECK(error.empty());

    for (double val : result.waveform_envelope) {
        CHECK(val >= 0.0);
        CHECK(val <= 1.0);
    }

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio stereo has correct channel count", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::analyze_audio(temp_file);

    CHECK(error.empty());
    CHECK(result.channels == 2);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio mono has correct channel count", "[analyzer]")
{
    std::string temp_file = create_temp_mono_wav(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::analyze_audio(temp_file);

    CHECK(error.empty());
    CHECK(result.channels == 1);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio sample rate is correct", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto [result, error] = mvlab::analyze_audio(temp_file);

    CHECK(error.empty());
    CHECK(result.sample_rate == 48000);

    cleanup_temp_file(temp_file);
}
