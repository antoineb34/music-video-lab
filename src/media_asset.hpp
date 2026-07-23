#ifndef MVLAB_MEDIA_ASSET_HPP
#define MVLAB_MEDIA_ASSET_HPP

#include "result.hpp"
#include <string>
#include <filesystem>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace mvlab {

struct Project;  // Forward declaration

using AssetId = std::string;

enum class MediaAssetType {
    audio,
    image,
    video
};

struct MediaAsset {
    AssetId id;
    MediaAssetType type;
    std::string display_name;
    std::filesystem::path relative_path;
    std::uintmax_t file_size;
};

// Converts a MediaAssetType enum value to its string representation.
std::string to_string(MediaAssetType type);

// Parses a string to a MediaAssetType. Returns invalid_argument if
// the string is not a recognized media type.
Result<MediaAssetType> parse_media_asset_type(const std::string& type_str);

// Validates a MediaAsset. Checks:
// - ID is not empty
// - display_name is not empty
// - relative_path is not empty
// - relative_path is not absolute
// - relative_path does not contain ".." path traversal
// - file_size is not zero
// Returns invalid_argument for any validation failure.
Result<void> validate_media_asset(const MediaAsset& asset);

inline void to_json(nlohmann::json& j, const MediaAsset& asset)
{
    j = nlohmann::json{
        {"id", asset.id},
        {"type", to_string(asset.type)},
        {"display_name", asset.display_name},
        {"relative_path", asset.relative_path.string()},
        {"file_size", asset.file_size}
    };
}

inline void from_json(const nlohmann::json& j, MediaAsset& asset)
{
    asset.id = j.at("id").get<std::string>();
    asset.display_name = j.at("display_name").get<std::string>();
    asset.relative_path = j.at("relative_path").get<std::string>();
    asset.file_size = j.at("file_size").get<std::uintmax_t>();

    std::string type_str = j.at("type").get<std::string>();
    auto type_result = parse_media_asset_type(type_str);
    if (!type_result) {
        throw std::runtime_error("Invalid media type: " + type_str);
    }
    asset.type = type_result.value();
}

// Generates a unique asset ID for the given project using a stable,
// readable format (asset-1, asset-2, etc.). Inspects existing asset IDs
// to avoid collisions. Safe after assets are removed.
// Returns invalid_argument if the generated ID fails validation.
AssetId generate_asset_id(const Project& project);

// Find a mutable asset by ID in the project.
// Returns invalid_argument if the requested ID is empty.
// Returns file_not_found if the asset ID does not exist.
Result<MediaAsset*> find_media_asset(
    Project& project,
    const AssetId& asset_id
);

// Find a const asset by ID in the project.
// Returns invalid_argument if the requested ID is empty.
// Returns file_not_found if the asset ID does not exist.
Result<const MediaAsset*> find_media_asset(
    const Project& project,
    const AssetId& asset_id
);

// Detects a media asset type from a source file's extension.
// Returns file_not_found if the source does not exist.
// Returns invalid_argument if the source is not a regular file or has an
// unsupported extension.
Result<MediaAssetType> detect_media_asset_type(
    const std::filesystem::path& source_path
);

// Planned destination metadata for a media asset.
struct MediaAssetDestination {
    std::filesystem::path absolute_path;  // full filesystem path
    std::filesystem::path relative_path;  // project-relative path (e.g. media/song.mp3)
};

// Plans a safe managed destination path inside a project's media/ directory.
// Does not create files or directories; returns planned paths only.
// Returns invalid_argument if project_root or source_path is empty.
// Returns file_not_found if source_path does not exist.
// Returns invalid_argument if source_path is not a regular file.
Result<MediaAssetDestination> plan_media_asset_destination(
    const std::filesystem::path& project_root,
    const std::filesystem::path& source_path
);

// Imports an external media file into an existing project with transaction safety.
// On success: file is copied, asset is appended, and project.json is saved.
// On failure: Project is unchanged, no partial files remain, and original project.json is valid.
// Returns invalid_argument for empty paths, unsupported formats, or invalid projects.
// Returns file_not_found for missing source or project root.
// Returns filesystem_error for copy or save failures.
// Returns malformed_project if the existing project is invalid.
Result<MediaAsset> import_media_asset(
    Project& project,
    const std::filesystem::path& project_root,
    const std::filesystem::path& source_path
);

// Removes a media asset from an existing project with transaction safety.
// On success: asset is removed from Project::assets, project.json is updated, and the
// managed media file is deleted. The removed asset is returned.
// On failure: Project is unchanged, project.json is restored if needed, and the
// managed file is not deleted.
// Returns invalid_argument for empty project root or asset ID, or for unsafe paths.
// Returns file_not_found for missing project root or unknown asset ID.
// Returns filesystem_error for project root not being a directory or save/delete failures.
// Returns malformed_project if the existing project is invalid.
// If the managed file is missing, the asset record is removed successfully (preferred behavior).
Result<MediaAsset> remove_media_asset(
    Project& project,
    const std::filesystem::path& project_root,
    const AssetId& asset_id
);

} // namespace mvlab

#endif // MVLAB_MEDIA_ASSET_HPP
