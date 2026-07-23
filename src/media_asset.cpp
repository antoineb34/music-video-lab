#include "media_asset.hpp"

namespace mvlab {

std::string to_string(MediaAssetType type)
{
    switch (type) {
        case MediaAssetType::audio:
            return "audio";
        case MediaAssetType::image:
            return "image";
        case MediaAssetType::video:
            return "video";
    }
    return "unknown";
}

Result<MediaAssetType> parse_media_asset_type(const std::string& type_str)
{
    if (type_str == "audio") {
        return MediaAssetType::audio;
    } else if (type_str == "image") {
        return MediaAssetType::image;
    } else if (type_str == "video") {
        return MediaAssetType::video;
    }

    return Error{
        ErrorCode::invalid_argument,
        "Unknown media type: " + type_str,
        std::nullopt
    };
}

Result<void> validate_media_asset(const MediaAsset& asset)
{
    if (asset.id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Asset ID cannot be empty",
            std::nullopt
        };
    }

    if (asset.display_name.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Asset display name cannot be empty",
            std::nullopt
        };
    }

    if (asset.relative_path.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Asset relative path cannot be empty",
            std::nullopt
        };
    }

    if (asset.relative_path.is_absolute()) {
        return Error{
            ErrorCode::invalid_argument,
            "Asset path must be relative, not absolute",
            std::nullopt
        };
    }

    // Check for ".." path traversal
    for (const auto& component : asset.relative_path) {
        if (component == "..") {
            return Error{
                ErrorCode::invalid_argument,
                "Asset path must not contain '..' path traversal",
                std::nullopt
            };
        }
    }

    if (asset.file_size == 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Asset file size must not be zero",
            std::nullopt
        };
    }

    return Result<void>{};
}

} // namespace mvlab
