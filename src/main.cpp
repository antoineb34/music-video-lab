#include "audio_inspector.hpp"
#include "audio_analyzer.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <string>
#include <iomanip>

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

    auto* analyze_cmd = audio_cmd->add_subcommand("analyze", "Analyze audio file samples");

    std::string analyze_file;
    int envelope_points = 100;
    analyze_cmd->add_option("file", analyze_file, "Path to audio file")->required();
    analyze_cmd->add_option("--points", envelope_points, "Waveform envelope points (default: 100)");

    analyze_cmd->callback([&analyze_file, &envelope_points]() {
        auto [result, error] = mvlab::analyze_audio(analyze_file, envelope_points);

        if (!error.empty()) {
            std::cerr << "Error: " << error << "\n";
            std::exit(1);
        }

        // Print in human-readable format
        std::cout << "File:              " << analyze_file << "\n";
        std::cout << "Total frames:      " << result.total_frames << "\n";
        std::cout << "Channels:          " << result.channels << "\n";
        std::cout << "Sample rate:       " << result.sample_rate << " Hz\n";
        std::cout << "Peak amplitude:    " << std::fixed << std::setprecision(4)
                  << result.peak_amplitude << "\n";
        std::cout << "RMS amplitude:     " << std::fixed << std::setprecision(4)
                  << result.rms_amplitude << "\n";
        std::cout << "Envelope points:   " << result.waveform_envelope.size() << "\n";

        if (!result.waveform_envelope.empty()) {
            std::cout << "Envelope:          ";
            for (size_t i = 0; i < result.waveform_envelope.size(); ++i) {
                if (i > 0 && i % 20 == 0) {
                    std::cout << "\n                   ";
                }
                std::cout << std::fixed << std::setprecision(2) << result.waveform_envelope[i];
                if (i < result.waveform_envelope.size() - 1) {
                    std::cout << " ";
                }
            }
            std::cout << "\n";
        }
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