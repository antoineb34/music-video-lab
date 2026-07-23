#ifndef MVLAB_MEDIA_ASSET_HPP
#define MVLAB_MEDIA_ASSET_HPP

#include "result.hpp"
#include <string>
#include <filesystem>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace mvlab {

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

} // namespace mvlab

#endif // MVLAB_MEDIA_ASSET_HPP
