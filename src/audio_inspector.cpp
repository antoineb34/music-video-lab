#include "audio_inspector.hpp"
#include <filesystem>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstdlib>

namespace mvlab {

std::pair<AudioInspectResult, std::string> inspect_audio(const std::string& file_path)
{
    AudioInspectResult result{};
    std::string error_output;

    // Check if file exists
    if (!std::filesystem::exists(file_path)) {
        return {result, "File not found: " + file_path};
    }

    // Create pipes for stdout and stderr
    int stdout_pipe[2];
    int stderr_pipe[2];

    if (pipe(stdout_pipe) == -1) {
        return {result, "Failed to create stdout pipe"};
    }
    if (pipe(stderr_pipe) == -1) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        return {result, "Failed to create stderr pipe"};
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return {result, "Failed to fork process"};
    }

    if (pid == 0) {
        // Child process
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);

        // Redirect stdout to pipe
        if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
            _exit(127);
        }
        // Redirect stderr to pipe
        if (dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
            _exit(127);
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
        _exit(127);
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
    while ((bytes_read = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
        error_output.append(buffer, bytes_read);
    }
    close(stderr_pipe[0]);

    // Wait for child
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        return {result, "Failed to wait for ffprobe process"};
    }

    if (!WIFEXITED(status)) {
        return {result, "ffprobe terminated abnormally"};
    }

    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0) {
        if (error_output.empty()) {
            return {result, "ffprobe failed with exit code " + std::to_string(exit_code)};
        }
        return {result, error_output};
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

    return {result, ""};
}

} // namespace mvlab
