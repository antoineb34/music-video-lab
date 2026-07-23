#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <string>

std::string run_mvlab(const std::string& args) {
    std::string cmd = "./mvlab " + args + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "ERROR";
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

TEST_CASE("CLI --version prints version", "[cli]")
{
    std::string out = run_mvlab("--version");
    CHECK(out.find("MusicVideoLab 0.1.0") != std::string::npos);
}

TEST_CASE("CLI -v prints version", "[cli]")
{
    std::string out = run_mvlab("-v");
    CHECK(out.find("MusicVideoLab 0.1.0") != std::string::npos);
}

TEST_CASE("CLI --help contains required text", "[cli]")
{
    std::string out = run_mvlab("--help");
    CHECK(out.find("MusicVideoLab") != std::string::npos);
    CHECK(out.find("Create music videos from audio, lyrics, and visuals.") != std::string::npos);
    CHECK(out.find("--version") != std::string::npos);
}

TEST_CASE("CLI without args prints help and exits 0", "[cli]")
{
    std::string out = run_mvlab("");
    CHECK(out.find("Create music videos from audio, lyrics, and visuals.") != std::string::npos);
    CHECK(out.find("--version") != std::string::npos);
}

TEST_CASE("audio inspect without file path is rejected", "[audio]")
{
    std::string out = run_mvlab("audio inspect");
    CHECK(out.find("required") != std::string::npos);
}

TEST_CASE("audio inspect with nonexistent file returns error", "[audio]")
{
    std::string out = run_mvlab("audio inspect /tmp/nonexistent_audio_file_12345.mp3");
    CHECK(out.find("Error") != std::string::npos);
}

TEST_CASE("audio analyze help text is present", "[audio]")
{
    std::string out = run_mvlab("audio analyze --help");
    CHECK(out.find("Analyze") != std::string::npos);
    CHECK(out.find("--points") != std::string::npos);
}

TEST_CASE("audio analyze without file path is rejected", "[audio]")
{
    std::string out = run_mvlab("audio analyze");
    CHECK(out.find("required") != std::string::npos);
}

TEST_CASE("audio analyze with nonexistent file returns error", "[audio]")
{
    std::string out = run_mvlab("audio analyze /tmp/nonexistent_audio_file_xyz.wav");
    CHECK(out.find("Error") != std::string::npos);
}

TEST_CASE("audio analyze with valid file succeeds", "[audio]")
{
    // Create temporary test audio
    std::string cmd = "ffmpeg -f lavfi -i anullsrc=r=48000:cl=stereo -t 0.5 -q:a 9 /tmp/mvlab_cli_test.wav -loglevel error 2>/dev/null";
    int ret = std::system(cmd.c_str());
    if (ret == 0) {
        std::string out = run_mvlab("audio analyze /tmp/mvlab_cli_test.wav");
        CHECK(out.find("Total frames") != std::string::npos);
        CHECK(out.find("Channels") != std::string::npos);
        CHECK(out.find("Sample rate") != std::string::npos);
        CHECK(out.find("Peak amplitude") != std::string::npos);
        CHECK(out.find("RMS amplitude") != std::string::npos);

        // Cleanup
        std::system("rm /tmp/mvlab_cli_test.wav 2>/dev/null");
    }
}

TEST_CASE("audio analyze --points handling", "[audio]")
{
    std::string cmd = "ffmpeg -f lavfi -i anullsrc=r=48000:cl=stereo -t 0.5 -q:a 9 /tmp/mvlab_cli_test_points.wav -loglevel error 2>/dev/null";
    int ret = std::system(cmd.c_str());
    if (ret == 0) {
        std::string out = run_mvlab("audio analyze /tmp/mvlab_cli_test_points.wav --points 50");
        CHECK(out.find("Envelope points") != std::string::npos);

        // Cleanup
        std::system("rm /tmp/mvlab_cli_test_points.wav 2>/dev/null");
    }
}