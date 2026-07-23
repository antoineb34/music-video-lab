#include "audio_analyzer.hpp"
#include "logger.hpp"
#include <filesystem>
#include <cstdint>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cmath>

namespace mvlab {

namespace fs = std::filesystem;

namespace {

// See audio_inspector.cpp for the same convention: 127 is execvp()'s
// standard "command not found" signal; 126 marks an internal setup
// failure (pipe/dup2) inside the forked child, distinct from a missing tool.
constexpr int kChildSetupFailureExitCode = 126;
constexpr int kExecFailedExitCode = 127;

} // namespace

Result<AudioAnalysisResult> analyze_audio(
    const std::string& file_path,
    int envelope_points)
{
    AudioAnalysisResult result{};

    // Validate inputs
    if (envelope_points <= 0) {
        return Error{ErrorCode::invalid_argument, "Points must be greater than zero", std::nullopt};
    }

    // Check if file exists
    if (!fs::exists(file_path)) {
        return Error{ErrorCode::file_not_found, "File not found: " + file_path, std::nullopt};
    }

    if (access(file_path.c_str(), R_OK) != 0) {
        return Error{ErrorCode::permission_denied, "Audio file is not readable: " + file_path, std::nullopt};
    }

    // Get file size to estimate if it's reasonable audio
    uintmax_t file_size = fs::file_size(file_path);
    if (file_size == 0) {
        return Error{ErrorCode::invalid_media, "Audio file is empty: " + file_path, std::nullopt};
    }

    // First, get audio metadata using ffprobe
    int probe_pipe[2];
    int probe_stderr_pipe[2];
    if (pipe(probe_pipe) == -1) {
        return Error{ErrorCode::internal_error, "Failed to create probe pipe", std::nullopt};
    }
    if (pipe(probe_stderr_pipe) == -1) {
        close(probe_pipe[0]);
        close(probe_pipe[1]);
        return Error{ErrorCode::internal_error, "Failed to create probe stderr pipe", std::nullopt};
    }

    Logger::instance().debug("Launching ffprobe (metadata probe) for " + file_path);

    pid_t probe_pid = fork();
    if (probe_pid == -1) {
        close(probe_pipe[0]);
        close(probe_pipe[1]);
        close(probe_stderr_pipe[0]);
        close(probe_stderr_pipe[1]);
        return Error{ErrorCode::internal_error, "Failed to fork ffprobe", std::nullopt};
    }

    if (probe_pid == 0) {
        // Child: ffprobe to get stream info
        close(probe_pipe[0]);
        close(probe_stderr_pipe[0]);

        if (dup2(probe_pipe[1], STDOUT_FILENO) == -1) {
            _exit(kChildSetupFailureExitCode);
        }
        if (dup2(probe_stderr_pipe[1], STDERR_FILENO) == -1) {
            _exit(kChildSetupFailureExitCode);
        }
        close(probe_pipe[1]);
        close(probe_stderr_pipe[1]);

        const char* probe_argv[] = {
            "ffprobe",
            "-v", "error",
            "-select_streams", "a:0",
            "-show_entries", "stream=sample_rate,channels",
            "-of", "default=noprint_wrappers=0:nokey=0",
            file_path.c_str(),
            nullptr
        };

        execvp("ffprobe", (char* const*)probe_argv);
        _exit(kExecFailedExitCode);
    }

    // Parent: read from probe
    close(probe_pipe[1]);
    close(probe_stderr_pipe[1]);

    std::string probe_output;
    char buffer[256];
    ssize_t bytes_read;
    while ((bytes_read = read(probe_pipe[0], buffer, sizeof(buffer))) > 0) {
        probe_output.append(buffer, bytes_read);
    }
    close(probe_pipe[0]);

    std::string probe_stderr;
    while ((bytes_read = read(probe_stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
        probe_stderr.append(buffer, bytes_read);
    }
    close(probe_stderr_pipe[0]);

    // Wait for ffprobe
    int probe_status;
    if (waitpid(probe_pid, &probe_status, 0) == -1) {
        return Error{ErrorCode::internal_error, "Failed to wait for ffprobe", std::nullopt};
    }

    if (!WIFEXITED(probe_status)) {
        return Error{ErrorCode::internal_error, "ffprobe terminated abnormally", std::nullopt};
    }

    int probe_exit_code = WEXITSTATUS(probe_status);
    if (probe_exit_code == kExecFailedExitCode) {
        Logger::instance().debug("ffprobe not found on PATH");
        return Error{ErrorCode::external_tool_unavailable, "ffprobe is not installed or not on PATH", std::nullopt};
    }
    if (probe_exit_code == kChildSetupFailureExitCode) {
        return Error{ErrorCode::internal_error, "Failed to prepare ffprobe process", std::nullopt};
    }
    if (probe_exit_code != 0) {
        Logger::instance().debug("ffprobe probe exited with code " + std::to_string(probe_exit_code) + " for " + file_path);
        return Error{
            ErrorCode::invalid_media,
            "Could not read audio format: " + file_path,
            probe_stderr.empty() ? std::nullopt : std::optional<std::string>(probe_stderr)
        };
    }

    // Parse probe output for sample_rate and channels
    int sample_rate = 0;
    int channels = 0;

    size_t pos = 0;
    while (pos < probe_output.size()) {
        size_t line_end = probe_output.find('\n', pos);
        if (line_end == std::string::npos) {
            line_end = probe_output.size();
        }

        std::string line = probe_output.substr(pos, line_end - pos);
        pos = line_end + 1;

        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);

            if (key == "sample_rate") {
                sample_rate = std::stoi(value);
            } else if (key == "channels") {
                channels = std::stoi(value);
            }
        }
    }

    if (sample_rate <= 0 || channels <= 0) {
        return Error{
            ErrorCode::malformed_external_output,
            "Could not determine audio format: " + file_path,
            probe_output.empty() ? std::nullopt : std::optional<std::string>(probe_output)
        };
    }

    result.sample_rate = sample_rate;
    result.channels = channels;

    // Now decode to raw PCM using ffmpeg
    int audio_pipe[2];
    int stderr_pipe[2];

    if (pipe(audio_pipe) == -1) {
        return Error{ErrorCode::internal_error, "Failed to create audio pipe", std::nullopt};
    }
    if (pipe(stderr_pipe) == -1) {
        close(audio_pipe[0]);
        close(audio_pipe[1]);
        return Error{ErrorCode::internal_error, "Failed to create stderr pipe", std::nullopt};
    }

    Logger::instance().debug("Launching ffmpeg decode for " + file_path);

    pid_t pid = fork();
    if (pid == -1) {
        close(audio_pipe[0]);
        close(audio_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return Error{ErrorCode::internal_error, "Failed to fork process", std::nullopt};
    }

    if (pid == 0) {
        // Child process: run ffmpeg to decode audio
        close(audio_pipe[0]);
        close(stderr_pipe[0]);

        // Redirect stdout to audio pipe
        if (dup2(audio_pipe[1], STDOUT_FILENO) == -1) {
            _exit(kChildSetupFailureExitCode);
        }
        // Redirect stderr to pipe (to suppress ffmpeg messages)
        if (dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
            _exit(kChildSetupFailureExitCode);
        }

        close(audio_pipe[1]);
        close(stderr_pipe[1]);

        // Build ffmpeg command to output raw PCM
        // ffmpeg -i input -f s16le -acodec pcm_s16le pipe:1
        const char* argv[] = {
            "ffmpeg",
            "-i", file_path.c_str(),
            "-f", "s16le",
            "-acodec", "pcm_s16le",
            "pipe:1",
            nullptr
        };

        execvp("ffmpeg", (char* const*)argv);
        _exit(kExecFailedExitCode);
    }

    // Parent process
    close(audio_pipe[1]);
    close(stderr_pipe[1]);

    // Read PCM data from ffmpeg
    std::vector<int16_t> pcm_samples;
    int16_t sample_buffer[4096];

    while ((bytes_read = read(audio_pipe[0], sample_buffer, sizeof(sample_buffer))) > 0) {
        // Each sample is 2 bytes (int16_t)
        int num_samples = bytes_read / 2;
        for (int i = 0; i < num_samples; ++i) {
            pcm_samples.push_back(sample_buffer[i]);
        }
    }
    close(audio_pipe[0]);

    // Read stderr (error messages)
    std::string stderr_data;
    while ((bytes_read = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
        stderr_data.append(buffer, bytes_read);
    }
    close(stderr_pipe[0]);

    // Wait for ffmpeg
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return Error{ErrorCode::internal_error, "Failed to wait for ffmpeg process", std::nullopt};
    }

    if (!WIFEXITED(status)) {
        return Error{ErrorCode::internal_error, "ffmpeg terminated abnormally", std::nullopt};
    }

    int exit_code = WEXITSTATUS(status);
    if (exit_code == kExecFailedExitCode) {
        Logger::instance().debug("ffmpeg not found on PATH");
        return Error{ErrorCode::external_tool_unavailable, "ffmpeg is not installed or not on PATH", std::nullopt};
    }
    if (exit_code == kChildSetupFailureExitCode) {
        return Error{ErrorCode::internal_error, "Failed to prepare ffmpeg process", std::nullopt};
    }
    if (exit_code != 0) {
        Logger::instance().debug("ffmpeg exited with code " + std::to_string(exit_code) + " for " + file_path);
        return Error{
            ErrorCode::invalid_media,
            "Failed to decode audio file: " + file_path,
            stderr_data.empty() ? std::nullopt : std::optional<std::string>(stderr_data)
        };
    }

    // Check if we got any PCM data
    if (pcm_samples.empty()) {
        return Error{ErrorCode::invalid_media, "No audio samples decoded from: " + file_path, std::nullopt};
    }

    // Calculate number of frames
    uint64_t num_frames = pcm_samples.size() / channels;
    if (pcm_samples.size() % channels != 0) {
        // Truncated PCM data
        return Error{
            ErrorCode::malformed_external_output,
            "Truncated or malformed PCM output from: " + file_path,
            std::nullopt
        };
    }

    result.total_frames = num_frames;

    // Calculate peak and RMS amplitude
    double peak = 0.0;
    double sum_squares = 0.0;

    for (int16_t sample : pcm_samples) {
        // Normalize to [-1.0, 1.0]
        double normalized = sample / 32768.0;
        double abs_val = std::abs(normalized);

        if (abs_val > peak) {
            peak = abs_val;
        }

        sum_squares += normalized * normalized;
    }

    result.peak_amplitude = peak;
    result.rms_amplitude = std::sqrt(sum_squares / pcm_samples.size());

    // Calculate waveform envelope
    result.waveform_envelope.resize(envelope_points, 0.0);

    if (num_frames > 0 && envelope_points > 0) {
        uint64_t samples_per_bucket = pcm_samples.size() / envelope_points;

        for (int i = 0; i < envelope_points; ++i) {
            uint64_t start_idx = i * samples_per_bucket;
            uint64_t end_idx = (i + 1) * samples_per_bucket;

            if (end_idx > static_cast<uint64_t>(pcm_samples.size())) {
                end_idx = pcm_samples.size();
            }

            double bucket_peak = 0.0;
            for (uint64_t j = start_idx; j < end_idx; ++j) {
                double normalized = std::abs(pcm_samples[j] / 32768.0);
                if (normalized > bucket_peak) {
                    bucket_peak = normalized;
                }
            }

            result.waveform_envelope[i] = bucket_peak;
        }
    }

    return result;
}

} // namespace mvlab
