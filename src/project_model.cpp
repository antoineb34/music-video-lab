#include "project_model.hpp"
#include <fstream>
#include <filesystem>

namespace mvlab {

std::pair<bool, std::string> validate_project(const Project& project)
{
    if (project.schema_version != 1) {
        return {false, "Unsupported schema version: " + std::to_string(project.schema_version)};
    }

    if (project.name.empty()) {
        return {false, "Project name cannot be empty"};
    }

    if (project.export_settings.width <= 0) {
        return {false, "Export width must be greater than zero"};
    }

    if (project.export_settings.height <= 0) {
        return {false, "Export height must be greater than zero"};
    }

    if (project.export_settings.fps <= 0) {
        return {false, "Export fps must be greater than zero"};
    }

    if (project.background.type.has_value()) {
        const auto& type = project.background.type.value();
        if (type != "image" && type != "video") {
            return {false, "Invalid background type: " + type};
        }
    }

    if (project.lyrics.format.has_value()) {
        const auto& format = project.lyrics.format.value();
        if (format != "txt" && format != "lrc") {
            return {false, "Invalid lyrics format: " + format};
        }
    }

    return {true, ""};
}

Project create_project(const std::string& name)
{
    Project project;
    project.schema_version = 1;
    project.name = name;
    project.audio = Audio{};
    project.background = Background{};
    project.lyrics = Lyrics{};
    project.export_settings = ExportSettings{1920, 1080, 30};
    return project;
}

std::pair<bool, std::string> save_project(const Project& project, const std::string& file_path)
{
    auto [is_valid, error] = validate_project(project);
    if (!is_valid) {
        return {false, error};
    }

    try {
        std::filesystem::path path(file_path);
        std::filesystem::create_directories(path.parent_path());

        nlohmann::json j = project;
        std::ofstream file(file_path);
        if (!file.is_open()) {
            return {false, "Failed to open file for writing: " + file_path};
        }
        file << j.dump(2) << std::endl;
        return {true, ""};
    } catch (const std::exception& e) {
        return {false, std::string("Exception while saving project: ") + e.what()};
    }
}

std::pair<Project, std::string> load_project(const std::string& file_path)
{
    Project project;

    try {
        std::filesystem::path path(file_path);
        if (!std::filesystem::exists(path)) {
            return {project, "Project file not found: " + file_path};
        }

        std::ifstream file(file_path);
        if (!file.is_open()) {
            return {project, "Failed to open file for reading: " + file_path};
        }

        nlohmann::json j;
        file >> j;

        project = j.get<Project>();

        auto [is_valid, error] = validate_project(project);
        if (!is_valid) {
            return {project, error};
        }

        return {project, ""};
    } catch (const nlohmann::json::exception& e) {
        return {project, std::string("JSON parsing error: ") + e.what()};
    } catch (const std::exception& e) {
        return {project, std::string("Exception while loading project: ") + e.what()};
    }
}

} // namespace mvlab
