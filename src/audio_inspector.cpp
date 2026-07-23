#include "audio_inspector.hpp"
#include "logger.hpp"
#include <filesystem>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdlib>

namespace mvlab {

namespace {

// Exit codes used by the forked child to signal setup problems distinct
// from execvp() itself failing (127 is the standard shell convention for
// "command not found", which we rely on to detect a missing ffprobe).
constexpr int kChildSetupFailureExitCode = 126;
constexpr int kExecFailedExitCode = 127;

} // namespace

Result<AudioInspectResult> inspect_audio(const std::string& file_path)
{
    AudioInspectResult result{};

    if (!std::filesystem::exists(file_path)) {
        return Error{ErrorCode::file_not_found, "Audio file not found: " + file_path, std::nullopt};
    }

    if (access(file_path.c_str(), R_OK) != 0) {
        return Error{ErrorCode::permission_denied, "Audio file is not readable: " + file_path, std::nullopt};
    }

    // Create pipes for stdout and stderr
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) == -1) {
        return Error{ErrorCode::internal_error, "Failed to create stdout pipe", std::nullopt};
    }
    if (pipe(stderr_pipe) == -1) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return Error{ErrorCode::internal_error, "Failed to create stderr pipe", std::nullopt};
    }

    Logger::instance().debug("Launching ffprobe for " + file_path);

    pid_t pid = fork();
    if (pid == -1) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return Error{ErrorCode::internal_error, "Failed to fork process", std::nullopt};
    }

    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        // Redirect stdout to pipe
        if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
            _exit(kChildSetupFailureExitCode);
        }
        // Redirect stderr to pipe
        if (dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
            _exit(kChildSetupFailureExitCode);
        }

        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Build argv
        const char* argv[] = {
            "ffprobe",
            "-v", "error",
            "-select_streams", "a:0",
            "-show_entries",
            "stream=codec_name,sample_rate,channels,bit_rate:format=duration,bit_rate",
            "-of", "default=noprint_wrappers=0:nokey=0",
            file_path.c_str(),
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
    char buffer[256];
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
        return Error{ErrorCode::internal_error, "Failed to wait for ffprobe process", std::nullopt};
    }

    if (!WIFEXITED(status)) {
        return Error{ErrorCode::internal_error, "ffprobe terminated abnormally", std::nullopt};
    }

    int exit_code = WEXITSTATUS(status);
    if (exit_code == kExecFailedExitCode) {
        Logger::instance().debug("ffprobe not found on PATH");
        return Error{
            ErrorCode::external_tool_unavailable,
            "ffprobe is not installed or not on PATH",
            std::nullopt
        };
    }
    if (exit_code == kChildSetupFailureExitCode) {
        return Error{ErrorCode::internal_error, "Failed to prepare ffprobe process", std::nullopt};
    }
    if (exit_code != 0) {
        Logger::instance().debug("ffprobe exited with code " + std::to_string(exit_code) + " for " + file_path);
        return Error{
            ErrorCode::invalid_media,
            "Invalid or unreadable audio file: " + file_path,
            error_output.empty() ? std::nullopt : std::optional<std::string>(error_output)
        };
    }

    // Parse stdout
    std::string current_section;
    size_t pos = 0;

    while (pos < stdout_data.size()) {
        size_t line_end = stdout_data.find('\n', pos);
        if (line_end == std::string::npos) {
            line_end = stdout_data.size();
        }

        std::string line = stdout_data.substr(pos, line_end - pos);
        pos = line_end + 1;

        if (line == "[STREAM]") {
            current_section = "STREAM";
        } else if (line == "[/STREAM]") {
            current_section = "";
        } else if (line == "[FORMAT]") {
            current_section = "FORMAT";
        } else if (line == "[/FORMAT]") {
            current_section = "";
        } else {
            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = line.substr(0, eq_pos);
                std::string value = line.substr(eq_pos + 1);

                if (current_section == "STREAM") {
                    if (key == "codec_name") {
                        result.codec = value;
                    } else if (key == "sample_rate") {
                        result.sample_rate = value;
                    } else if (key == "channels") {
                        result.channels = value;
                    } else if (key == "bit_rate") {
                        result.stream_bitrate = value;
                    }
                } else if (current_section == "FORMAT") {
                    if (key == "duration") {
                        result.duration = value;
                    } else if (key == "bit_rate") {
                        result.format_bitrate = value;
                    }
                }
            }
        }
    }

    if (result.codec.empty() && result.sample_rate.empty() && result.channels.empty()) {
        return Error{
            ErrorCode::malformed_external_output,
            "ffprobe did not report an audio stream for: " + file_path,
            stdout_data.empty() ? std::nullopt : std::optional<std::string>(stdout_data)
        };
    }

    return result;
}

} // namespace mvlab
