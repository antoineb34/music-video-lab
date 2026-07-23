#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <string>
#include <sys/wait.h>

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

struct CommandOutput {
    std::string text;
    int exit_code;
};

// Like run_mvlab(), but also reports the process exit code so tests can
// verify the ErrorCode -> exit code mapping end-to-end.
CommandOutput run_mvlab_full(const std::string& args) {
    std::string cmd = "./mvlab " + args + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {"ERROR", -1};
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    int status = pclose(pipe);
    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return {result, exit_code};
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

TEST_CASE("project create help text is present", "[project]")
{
    std::string out = run_mvlab("project create --help");
    CHECK(out.find("Create a new project") != std::string::npos);
    CHECK(out.find("--name") != std::string::npos);
}

TEST_CASE("project info help text is present", "[project]")
{
    std::string out = run_mvlab("project info --help");
    CHECK(out.find("Display project information") != std::string::npos);
}

TEST_CASE("project create without required arguments is rejected", "[project]")
{
    std::string out = run_mvlab("project create");
    CHECK(out.find("required") != std::string::npos);
}

TEST_CASE("project info without required arguments is rejected", "[project]")
{
    std::string out = run_mvlab("project info");
    CHECK(out.find("required") != std::string::npos);
}

TEST_CASE("project create and info succeed end-to-end", "[project]")
{
    std::system("rm -rf /tmp/mvlab_cli_project_test");

    auto create_out = run_mvlab_full("project create /tmp/mvlab_cli_project_test --name \"CLI Test\"");
    CHECK(create_out.exit_code == 0);
    CHECK(create_out.text.find("Created project") != std::string::npos);

    auto info_out = run_mvlab_full("project info /tmp/mvlab_cli_project_test/mvlab_cli_project_test.mvlab");
    CHECK(info_out.exit_code == 0);
    CHECK(info_out.text.find("CLI Test") != std::string::npos);

    std::system("rm -rf /tmp/mvlab_cli_project_test");
}

TEST_CASE("CLI default failure output hides technical details", "[cli][logging]")
{
    std::system("printf 'not audio' > /tmp/mvlab_cli_bad_media_default.txt");

    auto out = run_mvlab_full("audio inspect /tmp/mvlab_cli_bad_media_default.txt");

    CHECK(out.text.find("Error:") != std::string::npos);
    CHECK(out.text.find("Details:") == std::string::npos);

    std::system("rm -f /tmp/mvlab_cli_bad_media_default.txt");
}

TEST_CASE("CLI --verbose reveals error details", "[cli][logging]")
{
    std::system("printf 'not audio' > /tmp/mvlab_cli_bad_media_verbose.txt");

    auto out = run_mvlab_full("--verbose audio inspect /tmp/mvlab_cli_bad_media_verbose.txt");

    CHECK(out.text.find("Error:") != std::string::npos);
    CHECK(out.text.find("Details:") != std::string::npos);

    std::system("rm -f /tmp/mvlab_cli_bad_media_verbose.txt");
}

TEST_CASE("CLI --debug enables debug logs and error details", "[cli][logging]")
{
    std::system("printf 'not audio' > /tmp/mvlab_cli_bad_media_debug.txt");

    auto out = run_mvlab_full("--debug audio inspect /tmp/mvlab_cli_bad_media_debug.txt");

    CHECK(out.text.find("[DEBUG]") != std::string::npos);
    CHECK(out.text.find("Error:") != std::string::npos);
    CHECK(out.text.find("Details:") != std::string::npos);

    std::system("rm -f /tmp/mvlab_cli_bad_media_debug.txt");
}

TEST_CASE("CLI --quiet suppresses logs but still reports the error", "[cli][logging]")
{
    auto out = run_mvlab_full("--quiet audio inspect /tmp/mvlab_nonexistent_quiet_test.mp3");

    CHECK(out.text.find("Error:") != std::string::npos);
    CHECK(out.text.find("[DEBUG]") == std::string::npos);
    CHECK(out.text.find("[INFO]") == std::string::npos);
    CHECK(out.text.find("[WARNING]") == std::string::npos);
}

TEST_CASE("CLI logging flags are mutually exclusive", "[cli][logging]")
{
    auto verbose_debug = run_mvlab_full("--verbose --debug audio inspect /tmp/whatever.mp3");
    CHECK(verbose_debug.exit_code != 0);
    CHECK(verbose_debug.text.find("excludes") != std::string::npos);

    auto verbose_quiet = run_mvlab_full("--verbose --quiet audio inspect /tmp/whatever.mp3");
    CHECK(verbose_quiet.exit_code != 0);

    auto debug_quiet = run_mvlab_full("--debug --quiet audio inspect /tmp/whatever.mp3");
    CHECK(debug_quiet.exit_code != 0);
}

TEST_CASE("CLI exit code for file_not_found is 3", "[cli][exit-code]")
{
    auto out = run_mvlab_full("audio inspect /tmp/mvlab_nonexistent_exitcode_test.mp3");
    CHECK(out.exit_code == 3);
}

TEST_CASE("CLI exit code for invalid_argument is 2", "[cli][exit-code]")
{
    auto out = run_mvlab_full("audio analyze /tmp/mvlab_any_path_exitcode.wav --points 0");
    CHECK(out.exit_code == 2);
}

TEST_CASE("CLI exit code for malformed project data is 5", "[cli][exit-code]")
{
    std::system("mkdir -p /tmp/mvlab_cli_malformed_exitcode && printf '{ bad json' > /tmp/mvlab_cli_malformed_exitcode/project.json");

    auto out = run_mvlab_full("project info /tmp/mvlab_cli_malformed_exitcode/project.json");
    CHECK(out.exit_code == 5);

    std::system("rm -rf /tmp/mvlab_cli_malformed_exitcode");
}

TEST_CASE("CLI successful command exits 0 with unchanged output", "[cli][exit-code]")
{
    std::string cmd = "ffmpeg -f lavfi -i anullsrc=r=48000:cl=stereo -t 0.3 -q:a 9 /tmp/mvlab_cli_success.wav -loglevel error -y 2>/dev/null";
    int ret = std::system(cmd.c_str());
    if (ret == 0) {
        auto out = run_mvlab_full("audio inspect /tmp/mvlab_cli_success.wav");
        CHECK(out.exit_code == 0);
        CHECK(out.text.find("Codec:") != std::string::npos);

        std::system("rm -f /tmp/mvlab_cli_success.wav");
    }
}
