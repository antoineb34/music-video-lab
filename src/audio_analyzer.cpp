#include "audio_analyzer.hpp"
#include <filesystem>
#include <cstdint>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cmath>

namespace mvlab {

namespace fs = std::filesystem;

std::pair<AudioAnalysisResult, std::string> analyze_audio(
    const std::string& file_path,
    int envelope_points)
{
    AudioAnalysisResult result{};

    // Validate inputs
    if (envelope_points <= 0) {
        return {result, "Points must be greater than zero"};
    }

    // Check if file exists
    if (!fs::exists(file_path)) {
        return {result, "File not found: " + file_path};
    }

    // Get file size to estimate if it's reasonable audio
    uintmax_t file_size = fs::file_size(file_path);
    if (file_size == 0) {
        return {result, "File is empty"};
    }

    // First, get audio metadata using ffprobe
    int probe_pipe[2];
    if (pipe(probe_pipe) == -1) {
        return {result, "Failed to create probe pipe"};
    }

    pid_t probe_pid = fork();
    if (probe_pid == -1) {
        close(probe_pipe[0]);
        close(probe_pipe[1]);
        return {result, "Failed to fork ffprobe"};
    }

    if (probe_pid == 0) {
        // Child: ffprobe to get stream info
        close(probe_pipe[0]);

        if (dup2(probe_pipe[1], STDOUT_FILENO) == -1) {
            _exit(127);
        }
        close(probe_pipe[1]);

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
        _exit(127);
    }

    // Parent: read from probe
    close(probe_pipe[1]);

    std::string probe_output;
    char buffer[256];
    ssize_t bytes_read;
    while ((bytes_read = read(probe_pipe[0], buffer, sizeof(buffer))) > 0) {
        probe_output.append(buffer, bytes_read);
    }
    close(probe_pipe[0]);

    // Wait for ffprobe
    int probe_status;
    if (waitpid(probe_pid, &probe_status, 0) == -1) {
        return {result, "Failed to wait for ffprobe"};
    }

    if (!WIFEXITED(probe_status) || WEXITSTATUS(probe_status) != 0) {
        return {result, "Could not read audio format"};
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
        return {result, "Could not determine audio format"};
    }

    result.sample_rate = sample_rate;
    result.channels = channels;

    // Now decode to raw PCM using ffmpeg
    int audio_pipe[2];
    int stderr_pipe[2];

    if (pipe(audio_pipe) == -1) {
        return {result, "Failed to create audio pipe"};
    }
    if (pipe(stderr_pipe) == -1) {
        close(audio_pipe[0]);
        close(audio_pipe[1]);
        return {result, "Failed to create stderr pipe"};
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(audio_pipe[0]);
        close(audio_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return {result, "Failed to fork process"};
    }

    if (pid == 0) {
        // Child process: run ffmpeg to decode audio
        close(audio_pipe[0]);
        close(stderr_pipe[0]);

        // Redirect stdout to audio pipe
        if (dup2(audio_pipe[1], STDOUT_FILENO) == -1) {
            _exit(127);
        }
        // Redirect stderr to pipe (to suppress ffmpeg messages)
        if (dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
            _exit(127);
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
        _exit(127);
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
        return {result, "Failed to wait for ffmpeg process"};
    }

    if (!WIFEXITED(status)) {
        return {result, "ffmpeg terminated abnormally"};
    }

    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0) {
        if (!stderr_data.empty()) {
            return {result, "ffmpeg error"};
        }
        return {result, "ffmpeg failed with exit code " + std::to_string(exit_code)};
    }

    // Check if we got any PCM data
    if (pcm_samples.empty()) {
        return {result, "No audio samples decoded"};
    }

    // Calculate number of frames
    uint64_t num_frames = pcm_samples.size() / channels;
    if (pcm_samples.size() % channels != 0) {
        // Truncated PCM data
        return {result, "Truncated or malformed PCM output"};
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

    return {result, ""};
}

} // namespace mvlab
