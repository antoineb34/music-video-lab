#include "media_probe.hpp"
#include <nlohmann/json.hpp>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdlib>
#include <limits>

namespace mvlab {

std::string to_string(ProbedMediaKind kind)
{
    switch (kind) {
    case ProbedMediaKind::unknown:
        return "unknown";
    case ProbedMediaKind::image:
        return "image";
    case ProbedMediaKind::audio:
        return "audio";
    case ProbedMediaKind::video:
        return "video";
    case ProbedMediaKind::audio_video:
        return "audio_video";
    }
    return "unknown";
}

namespace {

constexpr int kChildSetupFailureExitCode = 126;
constexpr int kExecFailedExitCode = 127;

// Parse a decimal duration string into microseconds.
// Format: "123.456" or "123" or "0.000001"
// Requirements:
// - deterministic and locale-independent
// - reject negatives
// - reject NaN and infinity
// - reject malformed trailing content
// - detect overflow
// - rounding rule: truncate sub-microsecond fractional parts
Result<TimelineTime> parse_duration_string(std::string_view duration_str)
{
    if (duration_str.empty()) {
        return Error{
            ErrorCode::invalid_media,
            "Duration string is empty",
            std::nullopt
        };
    }

    // Reject negative values
    if (duration_str[0] == '-') {
        return Error{
            ErrorCode::invalid_media,
            "Duration cannot be negative",
            std::nullopt
        };
    }

    // Find decimal point
    size_t dot_pos = duration_str.find('.');
    if (dot_pos == std::string::npos) {
        // Integer seconds only
        dot_pos = duration_str.size();
    }

    // Parse integer part (seconds)
    std::string int_part(duration_str.substr(0, dot_pos));
    if (int_part.empty()) {
        return Error{
            ErrorCode::invalid_media,
            "Duration has no integer part",
            std::nullopt
        };
    }

    // Check for valid integer characters
    for (char c : int_part) {
        if (c < '0' || c > '9') {
            return Error{
                ErrorCode::invalid_media,
                "Duration has invalid characters in integer part",
                std::nullopt
            };
        }
    }

    // Parse integer seconds, checking for overflow
    std::int64_t seconds = 0;
    for (char c : int_part) {
        int digit = c - '0';
        // Check: seconds * 10 + digit won't overflow
        if (seconds > (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
            return Error{
                ErrorCode::invalid_media,
                "Duration integer part overflows",
                std::nullopt
            };
        }
        seconds = seconds * 10 + digit;
    }

    // Parse fractional part (if present)
    std::int64_t microseconds = 0;
    if (dot_pos < duration_str.size()) {
        std::string frac_part(duration_str.substr(dot_pos + 1));

        // Check for valid fractional characters and reject trailing content
        for (char c : frac_part) {
            if (c < '0' || c > '9') {
                return Error{
                    ErrorCode::invalid_media,
                    "Duration has invalid characters in fractional part",
                    std::nullopt
                };
            }
        }

        // Pad or truncate to 6 digits (microseconds)
        if (frac_part.length() > 6) {
            frac_part = frac_part.substr(0, 6);
        } else {
            frac_part.append(6 - frac_part.length(), '0');
        }

        // Parse as integer microseconds
        for (char c : frac_part) {
            microseconds = microseconds * 10 + (c - '0');
        }
    }

    // Convert to total microseconds, checking for overflow
    // total = seconds * 1_000_000 + microseconds
    if (seconds > (std::numeric_limits<std::int64_t>::max() - microseconds) / 1000000) {
        return Error{
            ErrorCode::invalid_media,
            "Duration overflows after conversion to microseconds",
            std::nullopt
        };
    }

    TimelineTime total_microseconds = seconds * 1000000 + microseconds;
    return total_microseconds;
}

// Parse a rational frame rate string like "24/1" or "30000/1001".
Result<RationalValue> parse_frame_rate_string(std::string_view rate_str)
{
    if (rate_str.empty()) {
        return Error{
            ErrorCode::invalid_media,
            "Frame rate string is empty",
            std::nullopt
        };
    }

    size_t slash_pos = rate_str.find('/');
    if (slash_pos == std::string::npos) {
        return Error{
            ErrorCode::invalid_media,
            "Frame rate missing denominator",
            std::nullopt
        };
    }

    std::string_view num_str = rate_str.substr(0, slash_pos);
    std::string_view den_str = rate_str.substr(slash_pos + 1);

    if (num_str.empty() || den_str.empty()) {
        return Error{
            ErrorCode::invalid_media,
            "Frame rate numerator or denominator is empty",
            std::nullopt
        };
    }

    // Parse numerator (can be negative, but we normalize to positive)
    std::int64_t numerator = 0;
    bool num_negative = false;
    size_t num_start = 0;
    if (num_str[0] == '-') {
        num_negative = true;
        num_start = 1;
    }

    for (size_t i = num_start; i < num_str.size(); ++i) {
        char c = num_str[i];
        if (c < '0' || c > '9') {
            return Error{
                ErrorCode::invalid_media,
                "Frame rate numerator has invalid characters",
                std::nullopt
            };
        }
        int digit = c - '0';
        if (numerator > (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
            return Error{
                ErrorCode::invalid_media,
                "Frame rate numerator overflows",
                std::nullopt
            };
        }
        numerator = numerator * 10 + digit;
    }

    if (num_negative) {
        numerator = -numerator;
    }

    // Parse denominator (must be positive)
    std::int64_t denominator = 0;
    bool den_negative = false;
    size_t den_start = 0;
    if (den_str[0] == '-') {
        den_negative = true;
        den_start = 1;
    }

    for (size_t i = den_start; i < den_str.size(); ++i) {
        char c = den_str[i];
        if (c < '0' || c > '9') {
            return Error{
                ErrorCode::invalid_media,
                "Frame rate denominator has invalid characters",
                std::nullopt
            };
        }
        int digit = c - '0';
        if (denominator > (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
            return Error{
                ErrorCode::invalid_media,
                "Frame rate denominator overflows",
                std::nullopt
            };
        }
        denominator = denominator * 10 + digit;
    }

    if (den_negative) {
        denominator = -denominator;
    }

    // Normalize signs: if denominator is negative, flip both
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }

    // Reject zero denominator
    if (denominator == 0) {
        return Error{
            ErrorCode::invalid_media,
            "Frame rate denominator cannot be zero",
            std::nullopt
        };
    }

    return RationalValue{numerator, denominator};
}

// Check if a codec is an image codec
bool is_image_codec(const std::string& codec_name)
{
    // Common image codecs
    return codec_name == "png" || codec_name == "jpeg" || codec_name == "jpg" ||
           codec_name == "bmp" || codec_name == "gif" || codec_name == "tiff" ||
           codec_name == "webp" || codec_name == "ppm" || codec_name == "pgm" ||
           codec_name == "pbm" || codec_name == "apng";
}

// Parse audio stream metadata from a JSON stream object
Result<AudioStreamMetadata> parse_audio_stream(const nlohmann::json& stream)
{
    try {
        AudioStreamMetadata audio;

        // Required: codec_name
        if (!stream.contains("codec_name") || !stream["codec_name"].is_string()) {
            return Error{
                ErrorCode::invalid_media,
                "Audio stream missing codec_name",
                std::nullopt
            };
        }
        audio.codec_name = stream["codec_name"].get<std::string>();

        // Required: sample_rate (string in ffprobe, convert to int)
        if (!stream.contains("sample_rate") || !stream["sample_rate"].is_string()) {
            return Error{
                ErrorCode::invalid_media,
                "Audio stream missing sample_rate",
                std::nullopt
            };
        }
        try {
            audio.sample_rate = std::stoi(stream["sample_rate"].get<std::string>());
        } catch (...) {
            return Error{
                ErrorCode::invalid_media,
                "Audio stream sample_rate is not a valid integer",
                std::nullopt
            };
        }

        // Required: channels
        if (!stream.contains("channels") || !stream["channels"].is_number_integer()) {
            return Error{
                ErrorCode::invalid_media,
                "Audio stream missing channels",
                std::nullopt
            };
        }
        audio.channels = stream["channels"].get<std::int32_t>();

        // Optional: channel_layout
        if (stream.contains("channel_layout") && stream["channel_layout"].is_string()) {
            audio.channel_layout = stream["channel_layout"].get<std::string>();
        }

        // Optional: bit_rate (string in ffprobe, convert to int)
        if (stream.contains("bit_rate") && stream["bit_rate"].is_string()) {
            try {
                audio.bit_rate = std::stoll(stream["bit_rate"].get<std::string>());
            } catch (...) {
                // Ignore invalid bit_rate
            }
        }

        return audio;
    } catch (const std::exception& e) {
        return Error{
            ErrorCode::invalid_media,
            "Failed to parse audio stream metadata",
            std::nullopt
        };
    }
}

// Parse video stream metadata from a JSON stream object
Result<VideoStreamMetadata> parse_video_stream(const nlohmann::json& stream)
{
    try {
        VideoStreamMetadata video;

        // Required: codec_name
        if (!stream.contains("codec_name") || !stream["codec_name"].is_string()) {
            return Error{
                ErrorCode::invalid_media,
                "Video stream missing codec_name",
                std::nullopt
            };
        }
        video.codec_name = stream["codec_name"].get<std::string>();

        // Required: width and height
        if (!stream.contains("width") || !stream["width"].is_number_integer()) {
            return Error{
                ErrorCode::invalid_media,
                "Video stream missing width",
                std::nullopt
            };
        }
        video.width = stream["width"].get<std::int32_t>();

        if (!stream.contains("height") || !stream["height"].is_number_integer()) {
            return Error{
                ErrorCode::invalid_media,
                "Video stream missing height",
                std::nullopt
            };
        }
        video.height = stream["height"].get<std::int32_t>();

        // Optional: frame rate (try avg_frame_rate first, then r_frame_rate)
        if (stream.contains("avg_frame_rate") && stream["avg_frame_rate"].is_string()) {
            auto rate_str = stream["avg_frame_rate"].get<std::string>();
            if (!rate_str.empty() && rate_str != "0/0") {
                auto rate_result = parse_frame_rate_string(rate_str);
                if (!rate_result) {
                    return rate_result.error();
                }
                video.frame_rate = rate_result.value();
            }
        }

        // Fallback to r_frame_rate if avg_frame_rate is unavailable or invalid
        if (!video.frame_rate && stream.contains("r_frame_rate") && stream["r_frame_rate"].is_string()) {
            auto rate_str = stream["r_frame_rate"].get<std::string>();
            if (!rate_str.empty() && rate_str != "0/0") {
                auto rate_result = parse_frame_rate_string(rate_str);
                if (!rate_result) {
                    return rate_result.error();
                }
                video.frame_rate = rate_result.value();
            }
        }

        // Optional: pixel_format
        if (stream.contains("pix_fmt") && stream["pix_fmt"].is_string()) {
            video.pixel_format = stream["pix_fmt"].get<std::string>();
        }

        // Optional: bit_rate (string in ffprobe, convert to int)
        if (stream.contains("bit_rate") && stream["bit_rate"].is_string()) {
            try {
                video.bit_rate = std::stoll(stream["bit_rate"].get<std::string>());
            } catch (...) {
                // Ignore invalid bit_rate
            }
        }

        return video;
    } catch (const std::exception& e) {
        return Error{
            ErrorCode::invalid_media,
            "Failed to parse video stream metadata",
            std::nullopt
        };
    }
}

}  // namespace

Result<MediaProbeResult> parse_ffprobe_json(
    const std::filesystem::path& source_path,
    std::string_view json_str)
{
    if (json_str.empty()) {
        return Error{
            ErrorCode::invalid_media,
            "ffprobe output is empty",
            std::nullopt
        };
    }

    MediaProbeResult result;
    result.source_path = source_path;

    try {
        auto json = nlohmann::json::parse(json_str);

        // Parse format metadata
        if (json.contains("format") && json["format"].is_object()) {
            const auto& format = json["format"];

            // Optional: duration (string in ffprobe)
            if (format.contains("duration") && format["duration"].is_string()) {
                auto duration_result = parse_duration_string(format["duration"].get<std::string>());
                if (!duration_result) {
                    return duration_result.error();
                }
                result.duration = duration_result.value();
            }

            // Optional: format_name
            if (format.contains("format_name") && format["format_name"].is_string()) {
                result.container_format = format["format_name"].get<std::string>();
            }

            // Optional: size
            if (format.contains("size") && format["size"].is_string()) {
                try {
                    result.file_size = std::stoll(format["size"].get<std::string>());
                } catch (...) {
                    // Ignore invalid size
                }
            }
        }

        // Parse streams - collect candidates and prefer defaults
        std::optional<AudioStreamMetadata> audio_metadata;
        std::optional<AudioStreamMetadata> audio_default_candidate;
        std::optional<VideoStreamMetadata> video_metadata;
        std::optional<VideoStreamMetadata> video_default_candidate;

        if (json.contains("streams") && json["streams"].is_array()) {
            for (const auto& stream : json["streams"]) {
                if (!stream.is_object()) {
                    continue;
                }

                if (!stream.contains("codec_type") || !stream["codec_type"].is_string()) {
                    continue;
                }

                std::string codec_type = stream["codec_type"].get<std::string>();

                // Check if stream is marked as default
                bool is_default = false;
                if (stream.contains("disposition") && stream["disposition"].is_object()) {
                    const auto& disp = stream["disposition"];
                    if (disp.contains("default") && disp["default"].is_number_integer()) {
                        is_default = disp["default"].get<int>() != 0;
                    }
                }

                // Handle audio streams - collect both default and first
                if (codec_type == "audio") {
                    auto audio_result = parse_audio_stream(stream);
                    if (!audio_result) {
                        return audio_result.error();
                    }

                    if (is_default && !audio_default_candidate) {
                        audio_default_candidate = audio_result.value();
                    } else if (!audio_metadata) {
                        audio_metadata = audio_result.value();
                    }
                }

                // Handle video streams (but not images)
                else if (codec_type == "video") {
                    // Skip if it's an image codec
                    if (stream.contains("codec_name") && stream["codec_name"].is_string()) {
                        std::string codec_name = stream["codec_name"].get<std::string>();
                        if (is_image_codec(codec_name)) {
                            // This might be a still image
                            if (!audio_metadata && !video_metadata && !audio_default_candidate && !video_default_candidate) {
                                result.kind = ProbedMediaKind::image;
                                return result;
                            }
                            continue;
                        }
                    }

                    auto video_result = parse_video_stream(stream);
                    if (!video_result) {
                        return video_result.error();
                    }

                    if (is_default && !video_default_candidate) {
                        video_default_candidate = video_result.value();
                    } else if (!video_metadata) {
                        video_metadata = video_result.value();
                    }
                }
            }
        }

        // Prefer default streams over first stream
        if (audio_default_candidate) {
            audio_metadata = audio_default_candidate;
        }
        if (video_default_candidate) {
            video_metadata = video_default_candidate;
        }

        // Assign parsed streams
        result.audio = audio_metadata;
        result.video = video_metadata;

        // Classify media type
        if (audio_metadata && video_metadata) {
            result.kind = ProbedMediaKind::audio_video;
        } else if (audio_metadata) {
            result.kind = ProbedMediaKind::audio;
        } else if (video_metadata) {
            result.kind = ProbedMediaKind::video;
        } else if (result.kind != ProbedMediaKind::image) {
            result.kind = ProbedMediaKind::unknown;
        }

        return result;
    } catch (const std::exception& e) {
        return Error{
            ErrorCode::invalid_media,
            "Failed to parse ffprobe JSON output",
            std::nullopt
        };
    }
}

Result<MediaProbeResult> probe_media_file(
    const std::filesystem::path& path,
    const MediaProbeOptions& options [[maybe_unused]])
{
    // Validate path
    if (path.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Media file path is empty",
            std::nullopt
        };
    }

    if (!std::filesystem::exists(path)) {
        return Error{
            ErrorCode::file_not_found,
            "Media file not found: " + path.string(),
            std::nullopt
        };
    }

    if (!std::filesystem::is_regular_file(path)) {
        return Error{
            ErrorCode::invalid_argument,
            "Path is not a regular file: " + path.string(),
            std::nullopt
        };
    }

    // Create pipes for stdout and stderr
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) == -1) {
        return Error{
            ErrorCode::internal_error,
            "Failed to create stdout pipe",
            std::nullopt
        };
    }
    if (pipe(stderr_pipe) == -1) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return Error{
            ErrorCode::internal_error,
            "Failed to create stderr pipe",
            std::nullopt
        };
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return Error{
            ErrorCode::internal_error,
            "Failed to fork process",
            std::nullopt
        };
    }

    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        // Redirect stdout and stderr
        if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
            _exit(kChildSetupFailureExitCode);
        }
        if (dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
            _exit(kChildSetupFailureExitCode);
        }

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Build argv
        const char* argv[] = {
            "ffprobe",
            "-v", "error",
            "-print_format", "json",
            "-show_format",
            "-show_streams",
            path.c_str(),
            nullptr
        };

        execvp("ffprobe", (char* const*)argv);
        _exit(kExecFailedExitCode);
    }

    // Parent process
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    // Read stdout
    std::string stdout_data;
    char buffer[4096];
    ssize_t bytes_read;
    while ((bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
        stdout_data.append(buffer, bytes_read);
    }
    close(stdout_pipe[0]);

    // Read stderr
    std::string error_output;
    while ((bytes_read = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
        error_output.append(buffer, bytes_read);
    }
    close(stderr_pipe[0]);

    // Wait for child
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return Error{
            ErrorCode::internal_error,
            "Failed to wait for ffprobe process",
            std::nullopt
        };
    }

    if (!WIFEXITED(status)) {
        return Error{
            ErrorCode::internal_error,
            "ffprobe terminated abnormally",
            std::nullopt
        };
    }

    int exit_code = WEXITSTATUS(status);
    if (exit_code == kExecFailedExitCode) {
        return Error{
            ErrorCode::external_tool_unavailable,
            "ffprobe is not installed or not on PATH",
            std::nullopt
        };
    }
    if (exit_code == kChildSetupFailureExitCode) {
        return Error{
            ErrorCode::internal_error,
            "Failed to prepare ffprobe process",
            std::nullopt
        };
    }
    if (exit_code != 0) {
        // Truncate error output for safety
        if (error_output.length() > 256) {
            error_output = error_output.substr(0, 256) + "...";
        }
        return Error{
            ErrorCode::external_tool_failed,
            "ffprobe failed for: " + path.string(),
            error_output.empty() ? std::nullopt : std::optional<std::string>(error_output)
        };
    }

    // Parse JSON output
    return parse_ffprobe_json(path, stdout_data);
}

}  // namespace mvlab
