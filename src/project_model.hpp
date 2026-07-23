#ifndef MVLAB_PROJECT_MODEL_HPP
#define MVLAB_PROJECT_MODEL_HPP

#include <string>
#include <optional>
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
        {"export_settings", proj.export_settings}
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
}

// Validate a project structure
// Returns pair<true, ""> if valid, or <false, error_message> if invalid
std::pair<bool, std::string> validate_project(const Project& project);

// Create a new project with default values
Project create_project(const std::string& name);

// Save project to JSON file
// Returns <true, ""> on success, or <false, error_message> on failure
std::pair<bool, std::string> save_project(const Project& project, const std::string& file_path);

// Load project from JSON file
// Returns <project, ""> on success, or <empty_project, error_message> on failure
std::pair<Project, std::string> load_project(const std::string& file_path);

} // namespace mvlab

#endif // MVLAB_PROJECT_MODEL_HPP
