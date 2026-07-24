#ifndef MVLAB_MEDIA_PROBE_HPP
#define MVLAB_MEDIA_PROBE_HPP

#include "result.hpp"
#include "timeline.hpp"
#include <string>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

namespace mvlab {

enum class ProbedMediaKind {
    unknown,
    image,
    audio,
    video,
    audio_video,
};

// Convert ProbedMediaKind to string representation.
std::string to_string(ProbedMediaKind kind);

// A rational value with numerator and denominator.
// Used for frame rates and other fractional values.
struct RationalValue {
    std::int64_t numerator = 0;
    std::int64_t denominator = 1;

    // Check if this rational is valid (denominator > 0).
    [[nodiscard]] bool is_valid() const noexcept { return denominator > 0; }

    // Check if this rational is zero (numerator == 0).
    [[nodiscard]] bool is_zero() const noexcept { return numerator == 0; }
};

// Audio stream metadata extracted from ffprobe.
struct AudioStreamMetadata {
    std::string codec_name;
    std::int32_t sample_rate = 0;
    std::int32_t channels = 0;
    std::optional<std::string> channel_layout;
    std::optional<std::int64_t> bit_rate;
};

// Video stream metadata extracted from ffprobe.
struct VideoStreamMetadata {
    std::string codec_name;
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::optional<RationalValue> frame_rate;
    std::optional<std::string> pixel_format;
    std::optional<std::int64_t> bit_rate;
};

// Complete metadata for a media file probed by ffprobe.
// All fields are owned values; no pointers or references to input.
struct MediaProbeResult {
    std::filesystem::path source_path;
    ProbedMediaKind kind = ProbedMediaKind::unknown;

    std::optional<TimelineTime> duration;  // in microseconds
    std::optional<AudioStreamMetadata> audio;
    std::optional<VideoStreamMetadata> video;

    std::optional<std::string> container_format;
    std::optional<std::int64_t> file_size;
};

// Options for probing media files.
struct MediaProbeOptions {
    std::filesystem::path ffprobe_executable{"ffprobe"};
};

// Probe a media file using ffprobe.
// Returns invalid_argument for empty path.
// Returns file_not_found for nonexistent or unreadable paths.
// Returns external_tool_unavailable if ffprobe is not found.
// Returns external_tool_failed if ffprobe fails.
// Returns invalid_media if ffprobe output is malformed or stream has bad values.
Result<MediaProbeResult> probe_media_file(
    const std::filesystem::path& path,
    const MediaProbeOptions& options = {});

// Parse ffprobe JSON output without invoking ffprobe.
// Useful for testing and JSON-only workloads.
// Returns invalid_media for malformed JSON or invalid metadata.
Result<MediaProbeResult> parse_ffprobe_json(
    const std::filesystem::path& source_path,
    std::string_view json);

} // namespace mvlab

#endif // MVLAB_MEDIA_PROBE_HPP
