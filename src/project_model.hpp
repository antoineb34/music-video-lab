#ifndef MVLAB_PROJECT_MODEL_HPP
#define MVLAB_PROJECT_MODEL_HPP

#include "result.hpp"
#include "media_asset.hpp"
#include "timeline.hpp"
#include "timeline_serialization.hpp"
#include <string>
#include <optional>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace mvlab {

// Current project schema version, written by to_json(Project). Schema
// version 1 predates timelines; version 2 adds the "timeline" field.
// validate_project() accepts both 1 (read-only, migrated in memory with an
// empty timeline) and 2; anything else is unsupported_schema.
inline constexpr int current_project_schema_version = 2;

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
    Timeline timeline;
};

// Thrown by Project's from_json when the "timeline" field fails to
// deserialize, so load_project() can distinguish an unsupported timeline
// schema version from other malformed timeline data - mirroring how
// load_project() already special-cases the project's own schema_version
// mismatch rather than collapsing it into a generic malformed_project.
class ProjectTimelineParseError : public std::runtime_error {
public:
    explicit ProjectTimelineParseError(Error error)
        : std::runtime_error(error.message), error_(std::move(error)) {}

    const Error& error() const { return error_; }

private:
    Error error_;
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
    // Defensive: save_project() always calls validate_project() (which
    // validates proj.timeline) before serializing, so this should not fail
    // in practice. If it ever does, throw rather than write a partial or
    // silently-dropped timeline.
    auto timeline_json = timeline_to_json(proj.timeline);
    if (!timeline_json) {
        throw std::runtime_error(
            "Failed to serialize project timeline: " + timeline_json.error().message);
    }

    j = nlohmann::json{
        {"schema_version", current_project_schema_version},
        {"name", proj.name},
        {"audio", proj.audio},
        {"background", proj.background},
        {"lyrics", proj.lyrics},
        {"export_settings", proj.export_settings},
        {"assets", proj.assets},
        {"timeline", timeline_json.value()}
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

    if (j.contains("timeline")) {
        auto timeline_result = timeline_from_json(j["timeline"]);
        if (!timeline_result) {
            throw ProjectTimelineParseError(timeline_result.error());
        }
        proj.timeline = timeline_result.value();
    } else {
        // Schema-v1 projects predate timelines: migrate in memory to an
        // empty, valid timeline. The literal schema_version value is still
        // checked by validate_project(), so an unsupported version is not
        // silently accepted just because "timeline" happens to be absent.
        proj.timeline = Timeline{};
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
