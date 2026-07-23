#include "media_probe.hpp"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

using namespace mvlab;

namespace {

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
