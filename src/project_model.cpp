#include "project_model.hpp"
#include "logger.hpp"
#include "media_asset.hpp"
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <unistd.h>

namespace mvlab {

namespace fs = std::filesystem;

Result<void> validate_project(const Project& project)
{
    if (project.schema_version != 1 && project.schema_version != current_project_schema_version) {
        return Error{
            ErrorCode::unsupported_schema,
            "Unsupported schema version: " + std::to_string(project.schema_version) +
                " (expected 1 or " + std::to_string(current_project_schema_version) + ")",
            std::nullopt
        };
    }

    if (project.name.empty()) {
        return Error{ErrorCode::invalid_argument, "Project name cannot be empty", std::nullopt};
    }

    if (project.export_settings.width <= 0) {
        return Error{ErrorCode::invalid_argument, "Export width must be greater than zero", std::nullopt};
    }

    if (project.export_settings.height <= 0) {
        return Error{ErrorCode::invalid_argument, "Export height must be greater than zero", std::nullopt};
    }

    if (project.export_settings.fps <= 0) {
        return Error{ErrorCode::invalid_argument, "Export fps must be greater than zero", std::nullopt};
    }

    if (project.background.type.has_value()) {
        const auto& type = project.background.type.value();
        if (type != "image" && type != "video") {
            return Error{ErrorCode::invalid_argument, "Invalid background type: " + type, std::nullopt};
        }
    }

    if (project.lyrics.format.has_value()) {
        const auto& format = project.lyrics.format.value();
        if (format != "txt" && format != "lrc") {
            return Error{ErrorCode::invalid_argument, "Invalid lyrics format: " + format, std::nullopt};
        }
    }

    std::unordered_set<std::string> seen_ids;
    for (const auto& asset : project.assets) {
        auto asset_validation = validate_media_asset(asset);
        if (!asset_validation) {
            return asset_validation.error();
        }

        if (seen_ids.find(asset.id) != seen_ids.end()) {
            return Error{
                ErrorCode::invalid_argument,
                "Duplicate asset ID: " + asset.id,
                std::nullopt
            };
        }
        seen_ids.insert(asset.id);
    }

    auto timeline_validation = validate_timeline(project.timeline);
    if (!timeline_validation) {
        return timeline_validation.error();
    }

    // Every media clip must reference an asset that actually exists in this
    // project; text clips need no asset. This cross-checks two fields that
    // validate_timeline() alone cannot see (it has no knowledge of
    // project.assets), so it lives here rather than in the timeline module.
    for (const auto& track : project.timeline.tracks) {
        for (const auto& clip : track.media_clips) {
            auto asset_result = find_media_asset(project, clip.asset_id);
            if (!asset_result) {
                return Error{
                    ErrorCode::invalid_argument,
                    "Media clip '" + clip.id + "' references unknown asset: " + clip.asset_id,
                    std::nullopt
                };
            }
        }
    }

    return Result<void>{};
}

Result<Project> create_project(const std::string& name)
{
    if (name.empty()) {
        return Error{ErrorCode::invalid_argument, "Project name cannot be empty", std::nullopt};
    }

    Project project;
    project.schema_version = current_project_schema_version;
    project.name = name;
    project.audio = Audio{};
    project.background = Background{};
    project.lyrics = Lyrics{};
    project.export_settings = ExportSettings{1920, 1080, 30};
    project.assets = std::vector<MediaAsset>{};
    project.timeline = Timeline{};
    return project;
}

Result<void> save_project(const Project& project, const std::string& file_path)
{
    auto validation = validate_project(project);
    if (!validation) {
        return validation.error();
    }

    fs::path path(file_path);

    try {
        fs::create_directories(path.parent_path());
    } catch (const fs::filesystem_error& e) {
        return Error{ErrorCode::filesystem_error, "Failed to create directory for: " + file_path, std::string(e.what())};
    }

    fs::path parent = path.parent_path();
    if (parent.empty()) {
        parent = ".";
    }
    if (access(parent.c_str(), W_OK) != 0) {
        return Error{ErrorCode::permission_denied, "Project directory is not writable: " + parent.string(), std::nullopt};
    }

    std::string serialized;
    try {
        nlohmann::json j = project;
        serialized = j.dump(2);
    } catch (const std::exception& e) {
        // Catches both nlohmann::json::exception and the defensive
        // std::runtime_error that Project's to_json() throws if timeline
        // serialization ever fails despite validate_project() above.
        return Error{ErrorCode::serialization_error, "Failed to serialize project data", std::string(e.what())};
    }

    Logger::instance().debug("Saving project to " + file_path);

    std::ofstream file(file_path);
    if (!file.is_open()) {
        return Error{ErrorCode::filesystem_error, "Failed to open file for writing: " + file_path, std::nullopt};
    }
    file << serialized << std::endl;
    if (!file.good()) {
        return Error{ErrorCode::filesystem_error, "Failed to write project file: " + file_path, std::nullopt};
    }

    return Result<void>{};
}

Result<Project> load_project(const std::string& file_path)
{
    fs::path path(file_path);

    if (!fs::exists(path)) {
        return Error{ErrorCode::file_not_found, "Project file not found: " + file_path, std::nullopt};
    }

    if (access(path.c_str(), R_OK) != 0) {
        return Error{ErrorCode::permission_denied, "Project file is not readable: " + file_path, std::nullopt};
    }

    Logger::instance().debug("Loading project from " + file_path);

    std::ifstream file(file_path);
    if (!file.is_open()) {
        return Error{ErrorCode::filesystem_error, "Failed to open file for reading: " + file_path, std::nullopt};
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::exception& e) {
        return Error{ErrorCode::malformed_project, "Malformed project JSON: " + file_path, std::string(e.what())};
    }

    Project project;
    try {
        project = j.get<Project>();
    } catch (const ProjectTimelineParseError& e) {
        // Mirrors the schema_version special-case below: an unsupported
        // timeline schema version is reported as such rather than
        // collapsed into a generic malformed_project.
        const Error& inner = e.error();
        if (inner.code == ErrorCode::unsupported_schema) {
            return inner;
        }
        return Error{ErrorCode::malformed_project, inner.message, inner.details};
    } catch (const std::exception& e) {
        return Error{ErrorCode::malformed_project, "Malformed project data: " + file_path, std::string(e.what())};
    }

    auto validation = validate_project(project);
    if (!validation) {
        const Error& inner = validation.error();
        if (inner.code == ErrorCode::unsupported_schema) {
            return inner;
        }
        // Data came from an external file, not a direct API call, so a
        // failed field check is reported as malformed project data.
        return Error{ErrorCode::malformed_project, inner.message, inner.details};
    }

    return project;
}

Result<ProjectPaths> create_project_on_disk(const fs::path& folder, const std::string& name)
{
    fs::path root = folder;
    if (!root.string().ends_with(".mvlab")) {
        root /= (root.filename().string() + ".mvlab");
    }

    if (fs::exists(root) && !fs::is_empty(root)) {
        return Error{
            ErrorCode::invalid_argument,
            "Project folder already exists and is not empty: " + root.string(),
            std::nullopt
        };
    }

    auto project = create_project(name);
    if (!project) {
        return project.error();
    }

    try {
        fs::create_directories(root / "media");
        fs::create_directories(root / "lyrics");
        fs::create_directories(root / "analysis");
        fs::create_directories(root / "renders");
    } catch (const fs::filesystem_error& e) {
        return Error{ErrorCode::filesystem_error, "Failed to create project folder structure: " + root.string(), std::string(e.what())};
    }

    fs::path project_file = root / "project.json";
    auto save_outcome = save_project(project.value(), project_file.string());
    if (!save_outcome) {
        return save_outcome.error();
    }

    Logger::instance().info("Created project '" + name + "' at " + root.string());

    return ProjectPaths{root, project_file};
}

} // namespace mvlab
