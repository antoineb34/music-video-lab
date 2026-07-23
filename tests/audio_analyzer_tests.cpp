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

    auto outcome = mvlab::analyze_audio(temp_file);

    REQUIRE(outcome.has_value());
    const auto& result = outcome.value();
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

    auto outcome = mvlab::analyze_audio(temp_file);

    REQUIRE(outcome.has_value());
    const auto& result = outcome.value();
    CHECK(result.channels == 1);
    CHECK(result.total_frames > 0);
    CHECK(result.sample_rate > 0);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio envelope point count matches --points", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto outcome_50 = mvlab::analyze_audio(temp_file, 50);
    REQUIRE(outcome_50.has_value());
    CHECK(outcome_50.value().waveform_envelope.size() == 50);

    auto outcome_200 = mvlab::analyze_audio(temp_file, 200);
    REQUIRE(outcome_200.has_value());
    CHECK(outcome_200.value().waveform_envelope.size() == 200);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio peak amplitude within range", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto outcome = mvlab::analyze_audio(temp_file);

    REQUIRE(outcome.has_value());
    CHECK(outcome.value().peak_amplitude >= 0.0);
    CHECK(outcome.value().peak_amplitude <= 1.0);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio RMS amplitude within range", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto outcome = mvlab::analyze_audio(temp_file);

    REQUIRE(outcome.has_value());
    CHECK(outcome.value().rms_amplitude >= 0.0);
    CHECK(outcome.value().rms_amplitude <= 1.0);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio silent audio returns near-zero peak and RMS", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto outcome = mvlab::analyze_audio(temp_file);

    REQUIRE(outcome.has_value());
    // Silence should have very low amplitude
    CHECK(outcome.value().peak_amplitude < 0.05);
    CHECK(outcome.value().rms_amplitude < 0.05);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio nonexistent file returns error", "[analyzer]")
{
    auto outcome = mvlab::analyze_audio("/tmp/mvlab_nonexistent_audio_xyz.wav");

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::file_not_found);
    CHECK(outcome.error().message.find("not found") != std::string::npos);
}

TEST_CASE("mvlab::analyze_audio invalid file returns typed invalid_media error", "[analyzer]")
{
    std::string temp_file = "/tmp/mvlab_analyze_invalid.txt";
    FILE* f = std::fopen(temp_file.c_str(), "w");
    std::fprintf(f, "This is not an audio file\n");
    std::fclose(f);

    auto outcome = mvlab::analyze_audio(temp_file);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_media);
    // ffprobe/ffmpeg stderr diagnostics must be preserved, not discarded.
    REQUIRE(outcome.error().details.has_value());
    CHECK_FALSE(outcome.error().details.value().empty());

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio points=0 is rejected", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto outcome = mvlab::analyze_audio(temp_file, 0);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(outcome.error().message.find("greater than zero") != std::string::npos);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio points=-1 is rejected", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto outcome = mvlab::analyze_audio(temp_file, -1);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);

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

    auto outcome = mvlab::analyze_audio(spaced_file);

    REQUIRE(outcome.has_value());
    CHECK(outcome.value().total_frames > 0);

    cleanup_temp_file(base_file);
    cleanup_temp_file(spaced_file);
}

TEST_CASE("mvlab::analyze_audio envelope values within range", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto outcome = mvlab::analyze_audio(temp_file);

    REQUIRE(outcome.has_value());

    for (double val : outcome.value().waveform_envelope) {
        CHECK(val >= 0.0);
        CHECK(val <= 1.0);
    }

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio stereo has correct channel count", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto outcome = mvlab::analyze_audio(temp_file);

    REQUIRE(outcome.has_value());
    CHECK(outcome.value().channels == 2);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio mono has correct channel count", "[analyzer]")
{
    std::string temp_file = create_temp_mono_wav(0.5);
    REQUIRE(!temp_file.empty());

    auto outcome = mvlab::analyze_audio(temp_file);

    REQUIRE(outcome.has_value());
    CHECK(outcome.value().channels == 1);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio sample rate is correct", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    auto outcome = mvlab::analyze_audio(temp_file);

    REQUIRE(outcome.has_value());
    CHECK(outcome.value().sample_rate == 48000);

    cleanup_temp_file(temp_file);
}

TEST_CASE("mvlab::analyze_audio reports external_tool_unavailable when ffmpeg is missing", "[analyzer]")
{
    std::string temp_file = create_temp_wav_analyzer(0.5);
    REQUIRE(!temp_file.empty());

    std::string empty_path_dir = "/tmp/mvlab_empty_path_analyze";
    fs::create_directories(empty_path_dir);

    const char* original_path = std::getenv("PATH");
    std::string saved_path = original_path ? original_path : "";
    setenv("PATH", empty_path_dir.c_str(), 1);

    auto outcome = mvlab::analyze_audio(temp_file);

    setenv("PATH", saved_path.c_str(), 1);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::external_tool_unavailable);

    cleanup_temp_file(temp_file);
    fs::remove_all(empty_path_dir);
}
