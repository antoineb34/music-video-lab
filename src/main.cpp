#include "audio_inspector.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    CLI::App app{"MusicVideoLab", "Create music videos from audio, lyrics, and visuals."};
    app.set_version_flag("-v,--version", "MusicVideoLab 0.1.0");

    auto* audio_cmd = app.add_subcommand("audio", "Audio-related commands");
    auto* inspect_cmd = audio_cmd->add_subcommand("inspect", "Inspect audio file properties");

    std::string audio_file;
    inspect_cmd->add_option("file", audio_file, "Path to audio file")->required();

    inspect_cmd->callback([&audio_file]() {
        auto [result, error] = mvlab::inspect_audio(audio_file);

        if (!error.empty()) {
            std::cerr << "Error: " << error << "\n";
            std::exit(1);
        }

        // Print in human-readable format
        std::cout << "File:        " << audio_file << "\n";
        std::cout << "Duration:    " << result.duration << " seconds\n";
        std::cout << "Codec:       " << result.codec << "\n";
        std::cout << "Sample rate: " << result.sample_rate << " Hz\n";
        std::cout << "Channels:    " << result.channels << "\n";

        // Prefer stream bitrate, fall back to format bitrate
        std::string bitrate = result.stream_bitrate;
        if (bitrate.empty() || bitrate == "N/A") {
            bitrate = result.format_bitrate;
        }
        if (bitrate.empty()) {
            bitrate = "unknown";
        }
        std::cout << "Bit rate:    " << bitrate << " bits/s\n";
    });

    try {
        CLI11_PARSE(app, argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    if (argc == 1) {
        std::cout << app.help() << '\n';
        return 0;
    }

    return 0;
}