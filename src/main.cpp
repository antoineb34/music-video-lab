#include "audio_inspector.hpp"
#include "audio_analyzer.hpp"
#include "project_model.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <string>
#include <iomanip>
#include <filesystem>

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

    auto* project_cmd = app.add_subcommand("project", "Project-related commands");
    auto* create_cmd = project_cmd->add_subcommand("create", "Create a new project");

    std::string create_folder;
    std::string project_name;
    create_cmd->add_option("folder", create_folder, "Project folder path")->required();
    create_cmd->add_option("--name", project_name, "Project display name")->required();

    create_cmd->callback([&create_folder, &project_name]() {
        std::filesystem::path project_path(create_folder);

        if (!project_path.string().ends_with(".mvlab")) {
            project_path /= (project_path.filename().string() + ".mvlab");
        }

        if (std::filesystem::exists(project_path)) {
            if (!std::filesystem::is_empty(project_path)) {
                std::cerr << "Error: Project folder already exists and is not empty: " << project_path << "\n";
                std::exit(1);
            }
        }

        try {
            std::filesystem::create_directories(project_path / "media");
            std::filesystem::create_directories(project_path / "lyrics");
            std::filesystem::create_directories(project_path / "analysis");
            std::filesystem::create_directories(project_path / "renders");

            auto project = mvlab::create_project(project_name);
            auto [success, error] = mvlab::save_project(project, (project_path / "project.json").string());

            if (!success) {
                std::cerr << "Error: " << error << "\n";
                std::exit(1);
            }

            std::cout << "Created project: " << project_path << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
            std::exit(1);
        }
    });

    auto* info_cmd = project_cmd->add_subcommand("info", "Display project information");

    std::string info_path;
    info_cmd->add_option("path", info_path, "Project folder or project.json file")->required();

    info_cmd->callback([&info_path]() {
        std::filesystem::path project_path(info_path);

        if (std::filesystem::is_directory(project_path)) {
            project_path /= "project.json";
        }

        auto [project, error] = mvlab::load_project(project_path.string());

        if (!error.empty()) {
            std::cerr << "Error: " << error << "\n";
            std::exit(1);
        }

        std::cout << "Schema version: " << project.schema_version << "\n";
        std::cout << "Name:           " << project.name << "\n";
        std::cout << "Audio:          " << (project.audio.path.has_value() ? project.audio.path.value() : "not set") << "\n";
        std::cout << "Background:     " << (project.background.path.has_value() ? project.background.path.value() : "not set");
        if (project.background.type.has_value()) {
            std::cout << " (" << project.background.type.value() << ")";
        }
        std::cout << "\n";
        std::cout << "Lyrics:         " << (project.lyrics.path.has_value() ? project.lyrics.path.value() : "not set");
        if (project.lyrics.format.has_value()) {
            std::cout << " (" << project.lyrics.format.value() << ")";
        }
        std::cout << "\n";
        std::cout << "Export:         " << project.export_settings.width << "x" << project.export_settings.height
                  << " @ " << project.export_settings.fps << " fps\n";
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