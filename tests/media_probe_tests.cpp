#include "media_probe.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

using namespace mvlab;

namespace {

// Helper: create a unique temporary directory for test files
std::string create_temp_dir()
{
    // Create a temporary directory using mkdtemp pattern
    std::string template_str = "/tmp/mvlab-media-probe-XXXXXX";
    char* dir = mkdtemp(&template_str[0]);
    if (!dir) {
        throw std::runtime_error("Failed to create temporary directory");
    }
    return std::string(dir);
}

// Helper: create a fake ffprobe executable script that outputs given JSON
std::filesystem::path create_fake_ffprobe(
    const std::string& temp_dir,
    const std::string& output_json,
    int exit_code = 0,
    const std::string& stderr_output = "")
{
    std::filesystem::path script_path = std::filesystem::path(temp_dir) / "ffprobe";

    std::ofstream script(script_path);
    script << "#!/bin/bash\n";

    if (!stderr_output.empty()) {
        // Escape the stderr output for shell safety
        script << "echo '" << stderr_output << "' >&2\n";
    }

    if (!output_json.empty()) {
        // Escape the JSON for shell safety
        script << "cat << 'EOF'\n" << output_json << "\nEOF\n";
    }

    script << "exit " << exit_code << "\n";
    script.close();

    // Make it executable
    chmod(script_path.c_str(), 0755);

    return script_path;
}

// Helper: create a fake ffprobe that records all arguments
std::filesystem::path create_arg_recording_ffprobe(
    const std::string& temp_dir,
    const std::string& arg_file)
{
    std::filesystem::path script_path = std::filesystem::path(temp_dir) / "ffprobe";

    std::ofstream script(script_path);
    script << "#!/bin/bash\n";
    script << "# Record all arguments\n";
    script << "{\n";
    script << "  for arg in \"$@\"; do\n";
    script << "    printf '%s\\n' \"$arg\"\n";
    script << "  done\n";
    script << "} > '" << arg_file << "'\n";
    script << "echo '{\"format\": {}, \"streams\": []}'\n";
    script << "exit 0\n";
    script.close();

    chmod(script_path.c_str(), 0755);

    return script_path;
}

// Helper: create a minimal valid ffprobe JSON for audio only
std::string audio_only_json()
{
    return R"({
        "format": {
            "duration": "10.5",
            "format_name": "mp3",
            "size": "1024000"
        },
        "streams": [
            {
                "codec_type": "audio",
                "codec_name": "mp3",
                "sample_rate": "44100",
                "channels": 2,
                "channel_layout": "stereo",
                "bit_rate": "128000",
                "disposition": {"default": 1}
            }
        ]
    })";
}

// Helper: create a minimal valid ffprobe JSON for video only
std::string video_only_json()
{
    return R"({
        "format": {
            "duration": "5.0",
            "format_name": "mp4",
            "size": "5120000"
        },
        "streams": [
            {
                "codec_type": "video",
                "codec_name": "h264",
                "width": 1920,
                "height": 1080,
                "avg_frame_rate": "30/1",
                "pix_fmt": "yuv420p",
                "bit_rate": "5000000",
                "disposition": {"default": 1}
            }
        ]
    })";
}

// Helper: create a minimal valid ffprobe JSON for audio + video
std::string audio_video_json()
{
    return R"({
        "format": {
            "duration": "10.0",
            "format_name": "mp4",
            "size": "10240000"
        },
        "streams": [
            {
                "codec_type": "audio",
                "codec_name": "aac",
                "sample_rate": "48000",
                "channels": 2,
                "bit_rate": "192000",
                "disposition": {"default": 1}
            },
            {
                "codec_type": "video",
                "codec_name": "h264",
                "width": 1920,
                "height": 1080,
                "avg_frame_rate": "30/1",
                "pix_fmt": "yuv420p",
                "bit_rate": "5000000",
                "disposition": {"default": 1}
            }
        ]
    })";
}

// Helper: image codec JSON
std::string image_json()
{
    return R"({
        "format": {
            "format_name": "image2",
            "size": "2048000"
        },
        "streams": [
            {
                "codec_type": "video",
                "codec_name": "png",
                "width": 1920,
                "height": 1080
            }
        ]
    })";
}

}  // namespace

// ===== JSON parsing =====

TEST_CASE("Parse audio-only JSON", "[media_probe][json_parsing]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp3", audio_only_json());
    REQUIRE(result);

    auto probe = result.value();
    CHECK(probe.kind == ProbedMediaKind::audio);
    REQUIRE(probe.audio.has_value());
    CHECK(probe.audio->codec_name == "mp3");
    CHECK(probe.audio->sample_rate == 44100);
    CHECK(probe.audio->channels == 2);
    REQUIRE(probe.audio->channel_layout.has_value());
    CHECK(probe.audio->channel_layout.value() == "stereo");
    CHECK(!probe.video.has_value());
}

TEST_CASE("Parse video-only JSON", "[media_probe][json_parsing]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp4", video_only_json());
    REQUIRE(result);

    auto probe = result.value();
    CHECK(probe.kind == ProbedMediaKind::video);
    REQUIRE(probe.video.has_value());
    CHECK(probe.video->codec_name == "h264");
    CHECK(probe.video->width == 1920);
    CHECK(probe.video->height == 1080);
    CHECK(!probe.audio.has_value());
}

TEST_CASE("Parse audio + video JSON", "[media_probe][json_parsing]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp4", audio_video_json());
    REQUIRE(result);

    auto probe = result.value();
    CHECK(probe.kind == ProbedMediaKind::audio_video);
    REQUIRE(probe.audio.has_value());
    REQUIRE(probe.video.has_value());
    CHECK(probe.audio->channels == 2);
    CHECK(probe.video->width == 1920);
}

TEST_CASE("Parse image JSON", "[media_probe][json_parsing]")
{
    auto result = parse_ffprobe_json("/tmp/test.png", image_json());
    REQUIRE(result);

    auto probe = result.value();
    CHECK(probe.kind == ProbedMediaKind::image);
}

TEST_CASE("Parse JSON with missing optional fields", "[media_probe][json_parsing]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "audio",
                "codec_name": "aac",
                "sample_rate": "48000",
                "channels": 2
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.aac", json_str);
    REQUIRE(result);

    auto probe = result.value();
    CHECK(probe.kind == ProbedMediaKind::audio);
    CHECK(!probe.duration.has_value());
    CHECK(!probe.container_format.has_value());
    CHECK(!probe.file_size.has_value());
    REQUIRE(probe.audio.has_value());
    CHECK(!probe.audio->channel_layout.has_value());
}

TEST_CASE("Reject empty JSON", "[media_probe][json_parsing]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp3", "");
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_media);
}

TEST_CASE("Reject malformed JSON", "[media_probe][json_parsing]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp3", "{invalid json}");
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_media);
}

TEST_CASE("Empty streams list", "[media_probe][json_parsing]")
{
    std::string json_str = R"({
        "format": {},
        "streams": []
    })";

    auto result = parse_ffprobe_json("/tmp/test.bin", json_str);
    REQUIRE(result);
    CHECK(result.value().kind == ProbedMediaKind::unknown);
}

TEST_CASE("Ignore subtitle streams", "[media_probe][json_parsing]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "subtitle",
                "codec_name": "subrip"
            },
            {
                "codec_type": "audio",
                "codec_name": "aac",
                "sample_rate": "48000",
                "channels": 2
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mkv", json_str);
    REQUIRE(result);
    CHECK(result.value().kind == ProbedMediaKind::audio);
}

TEST_CASE("Default audio stream preferred", "[media_probe][json_parsing]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "audio",
                "codec_name": "mp3",
                "sample_rate": "44100",
                "channels": 1,
                "disposition": {"default": 0}
            },
            {
                "codec_type": "audio",
                "codec_name": "aac",
                "sample_rate": "48000",
                "channels": 2,
                "disposition": {"default": 1}
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mkv", json_str);
    REQUIRE(result);
    REQUIRE(result.value().audio.has_value());
    CHECK(result.value().audio->codec_name == "aac");
}

TEST_CASE("Incorrect field types rejected", "[media_probe][json_parsing]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "audio",
                "codec_name": 123,
                "sample_rate": "48000",
                "channels": 2
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mp3", json_str);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_media);
}

// ===== Duration parsing =====

TEST_CASE("Parse integer seconds", "[media_probe][duration]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp3",
        R"({"format": {"duration": "10"}, "streams": []})");
    REQUIRE(result);
    REQUIRE(result.value().duration.has_value());
    CHECK(result.value().duration.value() == 10000000);  // 10 seconds in microseconds
}

TEST_CASE("Parse fractional seconds", "[media_probe][duration]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp3",
        R"({"format": {"duration": "10.5"}, "streams": []})");
    REQUIRE(result);
    REQUIRE(result.value().duration.has_value());
    CHECK(result.value().duration.value() == 10500000);
}

TEST_CASE("Parse zero duration", "[media_probe][duration]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp3",
        R"({"format": {"duration": "0"}, "streams": []})");
    REQUIRE(result);
    REQUIRE(result.value().duration.has_value());
    CHECK(result.value().duration.value() == 0);
}

TEST_CASE("Sub-microsecond duration truncates", "[media_probe][duration]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp3",
        R"({"format": {"duration": "1.0000005"}, "streams": []})");
    REQUIRE(result);
    REQUIRE(result.value().duration.has_value());
    // Fractional microseconds are truncated: 1.000000 seconds = 1_000_000 µs
    CHECK(result.value().duration.value() == 1000000);
}

TEST_CASE("Negative duration rejected", "[media_probe][duration]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp3",
        R"({"format": {"duration": "-5.0"}, "streams": []})");
    REQUIRE(!result);
}

TEST_CASE("Malformed duration rejected", "[media_probe][duration]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp3",
        R"({"format": {"duration": "10x5"}, "streams": []})");
    REQUIRE(!result);
}

// ===== Frame rate parsing =====

TEST_CASE("Parse 24/1 frame rate", "[media_probe][frame_rate]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "video",
                "codec_name": "h264",
                "width": 1920,
                "height": 1080,
                "avg_frame_rate": "24/1"
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mp4", json_str);
    REQUIRE(result);
    REQUIRE(result.value().video.has_value());
    REQUIRE(result.value().video->frame_rate.has_value());
    CHECK(result.value().video->frame_rate->numerator == 24);
    CHECK(result.value().video->frame_rate->denominator == 1);
}

TEST_CASE("Parse 30/1 frame rate", "[media_probe][frame_rate]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "video",
                "codec_name": "h264",
                "width": 1920,
                "height": 1080,
                "avg_frame_rate": "30/1"
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mp4", json_str);
    REQUIRE(result);
    REQUIRE(result.value().video->frame_rate.has_value());
    CHECK(result.value().video->frame_rate->numerator == 30);
    CHECK(result.value().video->frame_rate->denominator == 1);
}

TEST_CASE("Parse 30000/1001 frame rate (NTSC)", "[media_probe][frame_rate]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "video",
                "codec_name": "h264",
                "width": 1920,
                "height": 1080,
                "avg_frame_rate": "30000/1001"
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mp4", json_str);
    REQUIRE(result);
    REQUIRE(result.value().video->frame_rate.has_value());
    CHECK(result.value().video->frame_rate->numerator == 30000);
    CHECK(result.value().video->frame_rate->denominator == 1001);
}

TEST_CASE("0/0 frame rate treated as unavailable", "[media_probe][frame_rate]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "video",
                "codec_name": "h264",
                "width": 1920,
                "height": 1080,
                "avg_frame_rate": "0/0"
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mp4", json_str);
    REQUIRE(result);
    CHECK(!result.value().video->frame_rate.has_value());
}

TEST_CASE("Fallback to r_frame_rate when avg_frame_rate unavailable", "[media_probe][frame_rate]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "video",
                "codec_name": "h264",
                "width": 1920,
                "height": 1080,
                "avg_frame_rate": "0/0",
                "r_frame_rate": "24/1"
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mp4", json_str);
    REQUIRE(result);
    REQUIRE(result.value().video->frame_rate.has_value());
    CHECK(result.value().video->frame_rate->numerator == 24);
}

TEST_CASE("Zero denominator frame rate rejected", "[media_probe][frame_rate]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "video",
                "codec_name": "h264",
                "width": 1920,
                "height": 1080,
                "avg_frame_rate": "30/0"
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mp4", json_str);
    REQUIRE(!result);
}

// ===== Stream selection =====

TEST_CASE("Select first stream when no default", "[media_probe][stream_selection]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "audio",
                "codec_name": "mp3",
                "sample_rate": "44100",
                "channels": 1
            },
            {
                "codec_type": "audio",
                "codec_name": "aac",
                "sample_rate": "48000",
                "channels": 2
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mkv", json_str);
    REQUIRE(result);
    CHECK(result.value().audio->codec_name == "mp3");
}

// ===== Classification =====

TEST_CASE("Classify as audio", "[media_probe][classification]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp3", audio_only_json());
    REQUIRE(result);
    CHECK(result.value().kind == ProbedMediaKind::audio);
}

TEST_CASE("Classify as video", "[media_probe][classification]")
{
    auto result = parse_ffprobe_json("/tmp/test.mp4", video_only_json());
    REQUIRE(result);
    CHECK(result.value().kind == ProbedMediaKind::video);
}

TEST_CASE("Classify as audio_video", "[media_probe][classification]")
{
    auto result = parse_ffprobe_json("/tmp/test.mkv", audio_video_json());
    REQUIRE(result);
    CHECK(result.value().kind == ProbedMediaKind::audio_video);
}

TEST_CASE("Classify as image", "[media_probe][classification]")
{
    auto result = parse_ffprobe_json("/tmp/test.png", image_json());
    REQUIRE(result);
    CHECK(result.value().kind == ProbedMediaKind::image);
}

TEST_CASE("Classify as unknown", "[media_probe][classification]")
{
    std::string json_str = R"({"format": {}, "streams": []})";
    auto result = parse_ffprobe_json("/tmp/test.bin", json_str);
    REQUIRE(result);
    CHECK(result.value().kind == ProbedMediaKind::unknown);
}

// ===== API-level tests =====

TEST_CASE("Empty path rejected", "[media_probe][api]")
{
    auto result = probe_media_file("");
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Nonexistent path rejected", "[media_probe][api]")
{
    auto result = probe_media_file("/nonexistent/path/to/file.mp4");
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Directory path rejected", "[media_probe][api]")
{
    auto result = probe_media_file("/tmp");
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

// ===== Conversions =====

TEST_CASE("to_string(ProbedMediaKind) works", "[media_probe][conversions]")
{
    CHECK(to_string(ProbedMediaKind::audio) == "audio");
    CHECK(to_string(ProbedMediaKind::video) == "video");
    CHECK(to_string(ProbedMediaKind::audio_video) == "audio_video");
    CHECK(to_string(ProbedMediaKind::image) == "image");
    CHECK(to_string(ProbedMediaKind::unknown) == "unknown");
}

TEST_CASE("RationalValue::is_valid works", "[media_probe][conversions]")
{
    RationalValue valid{30, 1};
    CHECK(valid.is_valid());

    RationalValue invalid{30, 0};
    CHECK(!invalid.is_valid());

    RationalValue negative_den{30, -1};
    CHECK(!negative_den.is_valid());
}

TEST_CASE("RationalValue::is_zero works", "[media_probe][conversions]")
{
    RationalValue zero{0, 1};
    CHECK(zero.is_zero());

    RationalValue nonzero{24, 1};
    CHECK(!nonzero.is_zero());
}

// ===== Edge cases =====

TEST_CASE("Source path preserved in result", "[media_probe][edge_cases]")
{
    auto result = parse_ffprobe_json("/path/to/file.mp4", audio_only_json());
    REQUIRE(result);
    CHECK(result.value().source_path == "/path/to/file.mp4");
}

TEST_CASE("Missing format section handled", "[media_probe][edge_cases]")
{
    std::string json_str = R"({
        "streams": [
            {
                "codec_type": "audio",
                "codec_name": "aac",
                "sample_rate": "48000",
                "channels": 2
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.aac", json_str);
    REQUIRE(result);
    CHECK(!result.value().duration.has_value());
    CHECK(!result.value().container_format.has_value());
}

TEST_CASE("Audio stream missing required field rejected", "[media_probe][edge_cases]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "audio",
                "codec_name": "aac",
                "sample_rate": "48000"
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.aac", json_str);
    REQUIRE(!result);
}

TEST_CASE("Video stream missing required field rejected", "[media_probe][edge_cases]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "video",
                "codec_name": "h264",
                "width": 1920
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mp4", json_str);
    REQUIRE(!result);
}

TEST_CASE("Multiple audio streams selects first valid", "[media_probe][edge_cases]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "audio",
                "codec_name": "mp3",
                "sample_rate": "44100",
                "channels": 1
            },
            {
                "codec_type": "audio",
                "codec_name": "aac",
                "sample_rate": "48000",
                "channels": 2
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mkv", json_str);
    REQUIRE(result);
    CHECK(result.value().audio->codec_name == "mp3");
}

TEST_CASE("Multiple video streams selects first valid", "[media_probe][edge_cases]")
{
    std::string json_str = R"({
        "format": {},
        "streams": [
            {
                "codec_type": "video",
                "codec_name": "h264",
                "width": 1280,
                "height": 720
            },
            {
                "codec_type": "video",
                "codec_name": "hevc",
                "width": 1920,
                "height": 1080
            }
        ]
    })";

    auto result = parse_ffprobe_json("/tmp/test.mkv", json_str);
    REQUIRE(result);
    CHECK(result.value().video->width == 1280);
}

// ===== Process-facing tests =====

TEST_CASE("Probe missing ffprobe executable", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();
    std::filesystem::path nonexistent_exe = std::filesystem::path(temp_dir) / "nonexistent-ffprobe";

    // Create a real media file
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / "test.mp4";
    {
        std::ofstream f(media_file);
        f << "fake media content";
    }

    MediaProbeOptions options;
    options.ffprobe_executable = nonexistent_exe;

    auto result = probe_media_file(media_file, options);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::external_tool_unavailable);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Probe fake ffprobe success with audio JSON", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();

    // Create fake ffprobe that outputs valid audio JSON
    auto ffprobe_exe = create_fake_ffprobe(temp_dir, audio_only_json());

    // Create a real media file
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / "test.mp3";
    {
        std::ofstream f(media_file);
        f << "fake audio content";
    }

    MediaProbeOptions options;
    options.ffprobe_executable = ffprobe_exe;

    auto result = probe_media_file(media_file, options);
    REQUIRE(result);
    CHECK(result.value().kind == ProbedMediaKind::audio);
    REQUIRE(result.value().audio.has_value());
    CHECK(result.value().audio->channels == 2);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Probe fake ffprobe success with image JSON", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();

    // Create fake ffprobe that outputs valid image JSON
    auto ffprobe_exe = create_fake_ffprobe(temp_dir, image_json());

    // Create a real media file
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / "test.png";
    {
        std::ofstream f(media_file);
        f << "fake image content";
    }

    MediaProbeOptions options;
    options.ffprobe_executable = ffprobe_exe;

    auto result = probe_media_file(media_file, options);
    REQUIRE(result);
    CHECK(result.value().kind == ProbedMediaKind::image);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Probe fake ffprobe exits nonzero", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();

    // Create fake ffprobe that exits with code 1
    auto ffprobe_exe = create_fake_ffprobe(temp_dir, "", 1, "Error processing file");

    // Create a real media file
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / "test.mp4";
    {
        std::ofstream f(media_file);
        f << "fake media content";
    }

    MediaProbeOptions options;
    options.ffprobe_executable = ffprobe_exe;

    auto result = probe_media_file(media_file, options);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::external_tool_failed);
    REQUIRE(result.error().details.has_value());
    CHECK(result.error().details->find("Error processing file") != std::string::npos);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Probe fake ffprobe empty stdout rejected", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();

    // Create fake ffprobe that produces empty stdout
    auto ffprobe_exe = create_fake_ffprobe(temp_dir, "", 0);

    // Create a real media file
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / "test.mp4";
    {
        std::ofstream f(media_file);
        f << "fake media content";
    }

    MediaProbeOptions options;
    options.ffprobe_executable = ffprobe_exe;

    auto result = probe_media_file(media_file, options);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_media);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Probe fake ffprobe malformed JSON", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();

    // Create fake ffprobe that outputs invalid JSON
    auto ffprobe_exe = create_fake_ffprobe(temp_dir, "{invalid json}", 0);

    // Create a real media file
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / "test.mp4";
    {
        std::ofstream f(media_file);
        f << "fake media content";
    }

    MediaProbeOptions options;
    options.ffprobe_executable = ffprobe_exe;

    auto result = probe_media_file(media_file, options);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_media);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Probe stderr truncated to documented limit", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();

    // Create stderr output larger than 256 bytes
    std::string large_stderr(300, 'X');

    // Create fake ffprobe that produces large stderr and exits nonzero
    auto ffprobe_exe = create_fake_ffprobe(temp_dir, "", 1, large_stderr);

    // Create a real media file
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / "test.mp4";
    {
        std::ofstream f(media_file);
        f << "fake media content";
    }

    MediaProbeOptions options;
    options.ffprobe_executable = ffprobe_exe;

    auto result = probe_media_file(media_file, options);
    REQUIRE(!result);
    REQUIRE(result.error().details.has_value());

    // Verify truncation: should have "..." suffix and be around 256 + 3 chars
    const auto& details = result.error().details.value();
    CHECK(details.find("...") != std::string::npos);
    CHECK(details.length() <= 260);  // 256 + "..."

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Probe argument safety with special characters", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();

    // Create a filename with special characters
    std::string special_name = "test file 'quotes' $dollar [brackets] ; semicolon.mp4";
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / special_name;
    {
        std::ofstream f(media_file);
        f << "fake media content";
    }

    // Create arg-recording fake ffprobe
    std::string arg_file = std::filesystem::path(temp_dir).string() + "/args.txt";
    auto ffprobe_exe = create_arg_recording_ffprobe(temp_dir, arg_file);

    MediaProbeOptions options;
    options.ffprobe_executable = ffprobe_exe;

    auto result = probe_media_file(media_file, options);

    // Read recorded arguments
    std::ifstream args_in(arg_file);
    std::vector<std::string> recorded_args;
    std::string line;
    while (std::getline(args_in, line)) {
        recorded_args.push_back(line);
    }

    // Verify arguments
    // Expected (after argv[0]): "-v", "error", "-print_format", "json", "-show_format", "-show_streams", media_file_path
    // Bash $@ starts from argv[1], so we should have 7 arguments
    REQUIRE(recorded_args.size() >= 7);

    // The media path should be preserved exactly as the last argument
    CHECK(recorded_args.back() == media_file.string());

    // "-v" and "error" should be separate arguments
    auto v_it = std::find(recorded_args.begin(), recorded_args.end(), "-v");
    REQUIRE(v_it != recorded_args.end());
    REQUIRE(std::next(v_it) != recorded_args.end());
    CHECK(*std::next(v_it) == "error");

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Probe with trailing newline in JSON", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();

    // Create fake ffprobe with trailing newline
    std::string json_with_newline = audio_only_json() + "\n";
    auto ffprobe_exe = create_fake_ffprobe(temp_dir, json_with_newline);

    // Create a real media file
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / "test.mp3";
    {
        std::ofstream f(media_file);
        f << "fake audio content";
    }

    MediaProbeOptions options;
    options.ffprobe_executable = ffprobe_exe;

    auto result = probe_media_file(media_file, options);
    REQUIRE(result);
    CHECK(result.value().kind == ProbedMediaKind::audio);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Probe source path preserved in result", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();

    // Create fake ffprobe
    auto ffprobe_exe = create_fake_ffprobe(temp_dir, audio_only_json());

    // Create a real media file with specific path
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / "my-audio.mp3";
    {
        std::ofstream f(media_file);
        f << "fake audio content";
    }

    MediaProbeOptions options;
    options.ffprobe_executable = ffprobe_exe;

    auto result = probe_media_file(media_file, options);
    REQUIRE(result);
    CHECK(result.value().source_path == media_file);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("Probe repeated calls remain deterministic", "[media_probe][process]")
{
    auto temp_dir = create_temp_dir();

    // Create fake ffprobe
    auto ffprobe_exe = create_fake_ffprobe(temp_dir, audio_video_json());

    // Create a real media file
    std::filesystem::path media_file = std::filesystem::path(temp_dir) / "test.mkv";
    {
        std::ofstream f(media_file);
        f << "fake media content";
    }

    MediaProbeOptions options;
    options.ffprobe_executable = ffprobe_exe;

    auto result1 = probe_media_file(media_file, options);
    auto result2 = probe_media_file(media_file, options);

    REQUIRE(result1);
    REQUIRE(result2);
    CHECK(result1.value().kind == result2.value().kind);
    REQUIRE(result1.value().audio.has_value());
    REQUIRE(result2.value().audio.has_value());
    CHECK(result1.value().audio->channels == result2.value().audio->channels);

    // Cleanup
    std::filesystem::remove_all(temp_dir);
}
