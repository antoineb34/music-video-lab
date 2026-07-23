#include "audio_inspector.hpp"
#include "audio_analyzer.hpp"
#include "project_model.hpp"
#include "media_asset.hpp"
#include "error.hpp"
#include "result.hpp"
#include "logger.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <string>
#include <iomanip>
#include <filesystem>

namespace {

// Determines the logger's minimum level from the raw command-line
// arguments, before CLI11 parses anything. This must happen up front
// (rather than in a CLI11 callback) so that debug/info logging emitted
// from inside a subcommand's own callback - which CLI11 invokes as part
// of parsing - already reflects the requested verbosity. Mutual
// exclusivity between --verbose/--debug/--quiet is still enforced by
// CLI11 below; if the flags conflict, CLI11 rejects the parse before any
// subcommand callback (and therefore any logging) runs, so the exact
// level picked here in a conflicting case is never observed.
mvlab::LogLevel initial_log_level(int argc, char** argv)
{
    mvlab::LogLevel level = mvlab::LogLevel::warning;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--quiet") {
            level = mvlab::LogLevel::error;
        } else if (arg == "--verbose") {
            level = mvlab::LogLevel::info;
        } else if (arg == "--debug") {
            level = mvlab::LogLevel::debug;
        }
    }
    return level;
}

// Prints "Error: <message>" to stderr, and "Details: <details>" as well
// when the configured log level is verbose enough (--verbose or
// --debug) and details are available. This is the only place a failed
// Result is reported to the user, so it never duplicates the logger's
// own (separate, debug-level) diagnostic lines.
void report_error(const mvlab::Error& err)
{
    std::cerr << "Error: " << err.message << "\n";

    bool verbose_or_debug = static_cast<int>(mvlab::Logger::instance().min_level())
        >= static_cast<int>(mvlab::LogLevel::info);
    if (verbose_or_debug && err.details.has_value()) {
        std::cerr << "Details: " << err.details.value() << "\n";
    }
}

// Reports the error (if any) and exits with the stable code for its
// ErrorCode; otherwise returns the unwrapped value. Centralizes the
// report+exit sequence so every command follows the same contract.
template <typename T>
T unwrap_or_exit(mvlab::Result<T> result)
{
    if (!result) {
        report_error(result.error());
        std::exit(mvlab::exit_code_for(result.error().code));
    }
    return std::move(result).value();
}

// Resolves a project argument to the project.json path.
// Accepts either a directory path (MyProject.mvlab) or direct file path
// (MyProject.mvlab/project.json).
std::filesystem::path resolve_project_path(const std::string& project_arg)
{
    namespace fs = std::filesystem;
    fs::path project_path(project_arg);

    if (fs::is_directory(project_path)) {
        project_path /= "project.json";
    }

    return project_path;
}

} // namespace

int main(int argc, char** argv)
{
    mvlab::Logger::instance().set_min_level(initial_log_level(argc, argv));

    CLI::App app{"MusicVideoLab", "Create music videos from audio, lyrics, and visuals."};
    app.set_version_flag("-v,--version", "MusicVideoLab 0.1.0");

    bool verbose_flag = false;
    bool debug_flag = false;
    bool quiet_flag = false;
    auto* verbose_opt = app.add_flag("--verbose", verbose_flag, "Show info-level logs and error details")->group("Logging");
    auto* debug_opt = app.add_flag("--debug", debug_flag, "Show debug-level logs and error details")->group("Logging");
    auto* quiet_opt = app.add_flag("--quiet", quiet_flag, "Suppress warning/info/debug logs")->group("Logging");
    verbose_opt->excludes(debug_opt);
    verbose_opt->excludes(quiet_opt);
    debug_opt->excludes(quiet_opt);

    auto* audio_cmd = app.add_subcommand("audio", "Audio-related commands");
    auto* inspect_cmd = audio_cmd->add_subcommand("inspect", "Inspect audio file properties");

    std::string audio_file;
    inspect_cmd->add_option("file", audio_file, "Path to audio file")->required();

    inspect_cmd->callback([&audio_file]() {
        auto result = unwrap_or_exit(mvlab::inspect_audio(audio_file));

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
        auto result = unwrap_or_exit(mvlab::analyze_audio(analyze_file, envelope_points));

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
        auto paths = unwrap_or_exit(mvlab::create_project_on_disk(create_folder, project_name));
        std::cout << "Created project: " << paths.root.string() << "\n";
    });

    auto* info_cmd = project_cmd->add_subcommand("info", "Display project information");

    std::string info_path;
    info_cmd->add_option("path", info_path, "Project folder or project.json file")->required();

    info_cmd->callback([&info_path]() {
        std::filesystem::path project_path(info_path);

        if (std::filesystem::is_directory(project_path)) {
            project_path /= "project.json";
        }

        auto project = unwrap_or_exit(mvlab::load_project(project_path.string()));

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

    auto* asset_cmd = project_cmd->add_subcommand("asset", "Project media asset commands");

    // Asset import command
    auto* import_asset_cmd = asset_cmd->add_subcommand("import", "Import a media file into a project");
    std::string import_project;
    std::string import_source;
    import_asset_cmd->add_option("project", import_project, "Project folder or project.json file")->required();
    import_asset_cmd->add_option("source", import_source, "Path to media file to import")->required();

    import_asset_cmd->callback([&import_project, &import_source]() {
        std::filesystem::path project_file = resolve_project_path(import_project);
        std::filesystem::path project_root = project_file.parent_path();

        auto project = unwrap_or_exit(mvlab::load_project(project_file.string()));
        auto imported = unwrap_or_exit(mvlab::import_media_asset(project, project_root, import_source));

        std::cout << "Imported asset\n";
        std::cout << "ID:            " << imported.id << "\n";
        std::cout << "Type:          " << mvlab::to_string(imported.type) << "\n";
        std::cout << "Display name:  " << imported.display_name << "\n";
        std::cout << "Path:          " << imported.relative_path.string() << "\n";
        std::cout << "Size:          " << imported.file_size << " bytes\n";
    });

    // Asset list command
    auto* list_asset_cmd = asset_cmd->add_subcommand("list", "List all assets in a project");
    std::string list_project;
    list_asset_cmd->add_option("project", list_project, "Project folder or project.json file")->required();

    list_asset_cmd->callback([&list_project]() {
        std::filesystem::path project_file = resolve_project_path(list_project);
        auto project = unwrap_or_exit(mvlab::load_project(project_file.string()));

        std::cout << "Assets: " << project.assets.size() << "\n";
        for (const auto& asset : project.assets) {
            std::cout << asset.id << "  "
                     << mvlab::to_string(asset.type) << "  "
                     << asset.display_name << "  "
                     << asset.relative_path.string() << "  "
                     << asset.file_size << " bytes\n";
        }
    });

    // Asset info command
    auto* info_asset_cmd = asset_cmd->add_subcommand("info", "Display asset information");
    std::string info_asset_project;
    std::string asset_id;
    info_asset_cmd->add_option("project", info_asset_project, "Project folder or project.json file")->required();
    info_asset_cmd->add_option("asset-id", asset_id, "Asset ID")->required();

    info_asset_cmd->callback([&info_asset_project, &asset_id]() {
        std::filesystem::path project_file = resolve_project_path(info_asset_project);
        auto project = unwrap_or_exit(mvlab::load_project(project_file.string()));

        auto asset_ptr = unwrap_or_exit(mvlab::find_media_asset(project, asset_id));

        std::cout << "ID:            " << asset_ptr->id << "\n";
        std::cout << "Type:          " << mvlab::to_string(asset_ptr->type) << "\n";
        std::cout << "Display name:  " << asset_ptr->display_name << "\n";
        std::cout << "Path:          " << asset_ptr->relative_path.string() << "\n";
        std::cout << "Size:          " << asset_ptr->file_size << " bytes\n";
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
