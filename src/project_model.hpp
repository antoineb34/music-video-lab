#ifndef MVLAB_PROJECT_MODEL_HPP
#define MVLAB_PROJECT_MODEL_HPP

#include "result.hpp"
#include "media_asset.hpp"
#include <string>
#include <optional>
#include <vector>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace mvlab {

struct AudioAnalysisSummary {
    int sample_rate = 0;
    int channels = 0;
    uint64_t total_frames = 0;
    double peak_amplitude = 0.0;
    double rms_amplitude = 0.0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        AudioAnalysisSummary,
        sample_rate,
        channels,
        total_frames,
        peak_amplitude,
        rms_amplitude
    )
};

struct Audio {
    std::optional<std::string> path;
    std::optional<AudioAnalysisSummary> analysis;
};

struct Background {
    std::optional<std::string> path;
    std::optional<std::string> type;
};

struct Lyrics {
    std::optional<std::string> path;
    std::optional<std::string> format;
};

struct ExportSettings {
    int width = 1920;
    int height = 1080;
    int fps = 30;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        ExportSettings,
        width,
        height,
        fps
    )
};

struct Project {
    int schema_version = 1;
    std::string name;
    Audio audio;
    Background background;
    Lyrics lyrics;
    ExportSettings export_settings;
    std::vector<MediaAsset> assets;
};

inline void to_json(nlohmann::json& j, const Audio& audio)
{
    j = nlohmann::json::object();
    if (audio.path.has_value()) {
        j["path"] = audio.path.value();
    }
    if (audio.analysis.has_value()) {
        j["analysis"] = audio.analysis.value();
    }
}

inline void from_json(const nlohmann::json& j, Audio& audio)
{
    if (j.contains("path") && !j["path"].is_null()) {
        audio.path = j["path"].get<std::string>();
    }
    if (j.contains("analysis") && !j["analysis"].is_null()) {
        audio.analysis = j["analysis"].get<AudioAnalysisSummary>();
    }
}

inline void to_json(nlohmann::json& j, const Background& bg)
{
    j = nlohmann::json::object();
    if (bg.path.has_value()) {
        j["path"] = bg.path.value();
    }
    if (bg.type.has_value()) {
        j["type"] = bg.type.value();
    }
}

inline void from_json(const nlohmann::json& j, Background& bg)
{
    if (j.contains("path") && !j["path"].is_null()) {
        bg.path = j["path"].get<std::string>();
    }
    if (j.contains("type") && !j["type"].is_null()) {
        bg.type = j["type"].get<std::string>();
    }
}

inline void to_json(nlohmann::json& j, const Lyrics& lyrics)
{
    j = nlohmann::json::object();
    if (lyrics.path.has_value()) {
        j["path"] = lyrics.path.value();
    }
    if (lyrics.format.has_value()) {
        j["format"] = lyrics.format.value();
    }
}

inline void from_json(const nlohmann::json& j, Lyrics& lyrics)
{
    if (j.contains("path") && !j["path"].is_null()) {
        lyrics.path = j["path"].get<std::string>();
    }
    if (j.contains("format") && !j["format"].is_null()) {
        lyrics.format = j["format"].get<std::string>();
    }
}

inline void to_json(nlohmann::json& j, const Project& proj)
{
    j = nlohmann::json{
        {"schema_version", proj.schema_version},
        {"name", proj.name},
        {"audio", proj.audio},
        {"background", proj.background},
        {"lyrics", proj.lyrics},
        {"export_settings", proj.export_settings},
        {"assets", proj.assets}
    };
}

inline void from_json(const nlohmann::json& j, Project& proj)
{
    if (j.contains("schema_version")) {
        proj.schema_version = j["schema_version"].get<int>();
    }
    proj.name = j["name"].get<std::string>();
    if (j.contains("audio")) {
        proj.audio = j["audio"].get<Audio>();
    }
    if (j.contains("background")) {
        proj.background = j["background"].get<Background>();
    }
    if (j.contains("lyrics")) {
        proj.lyrics = j["lyrics"].get<Lyrics>();
    }
    if (j.contains("export_settings")) {
        proj.export_settings = j["export_settings"].get<ExportSettings>();
    }
    if (j.contains("assets")) {
        proj.assets = j["assets"].get<std::vector<MediaAsset>>();
    }
}

// Validates a Project supplied directly by a caller (in-memory). Field
// problems are reported as invalid_argument; an unrecognized schema
// version is always reported as unsupported_schema regardless of caller
// context. load_project() remaps invalid_argument to malformed_project
// when the same checks fail against data read from a file, since in that
// case the bad data originated externally rather than from a direct call.
Result<void> validate_project(const Project& project);

// Create a new project with default values. Fails with invalid_argument
// if name is empty.
Result<Project> create_project(const std::string& name);

// Save project to JSON file. Validates first (see validate_project).
Result<void> save_project(const Project& project, const std::string& file_path);

// Load project from JSON file.
Result<Project> load_project(const std::string& file_path);

// Paths that make up an initialized project folder on disk.
struct ProjectPaths {
    std::filesystem::path root;          // the <name>.mvlab folder
    std::filesystem::path project_file;  // root / "project.json"
};

// Creates the full <name>.mvlab folder structure (media/, lyrics/,
// analysis/, renders/, project.json) at `folder`, appending the .mvlab
// suffix if `folder` doesn't already end with it. Refuses to overwrite an
// existing non-empty project folder.
Result<ProjectPaths> create_project_on_disk(const std::filesystem::path& folder, const std::string& name);

} // namespace mvlab

#endif // MVLAB_PROJECT_MODEL_HPP
