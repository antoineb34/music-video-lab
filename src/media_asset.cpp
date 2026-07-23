#include "media_asset.hpp"
#include "project_model.hpp"
#include <regex>
#include <set>
#include <algorithm>
#include <cctype>

namespace mvlab {

namespace {

// Convert a string to lowercase in-place
std::string to_lowercase(std::string str)
{
    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return str;
}

// Check if an extension (with leading dot) is a supported audio extension
bool is_audio_extension(const std::string& ext)
{
    static const std::set<std::string> audio_exts{
        ".mp3", ".wav", ".flac", ".ogg", ".m4a", ".aac"
    };
    return audio_exts.count(to_lowercase(ext)) > 0;
}

// Check if an extension (with leading dot) is a supported image extension
bool is_image_extension(const std::string& ext)
{
    static const std::set<std::string> image_exts{
        ".png", ".jpg", ".jpeg", ".webp", ".bmp"
    };
    return image_exts.count(to_lowercase(ext)) > 0;
}

// Check if an extension (with leading dot) is a supported video extension
bool is_video_extension(const std::string& ext)
{
    static const std::set<std::string> video_exts{
        ".mp4", ".mkv", ".mov", ".webm", ".avi"
    };
    return video_exts.count(to_lowercase(ext)) > 0;
}

// Sanitize a filename stem: keep letters, digits, -, _; replace runs of
// unsupported chars with single -; trim leading/trailing separators; use
// "asset" if result is empty.
std::string sanitize_stem(const std::string& stem)
{
    if (stem.empty()) {
        return "asset";
    }

    std::string result;
    bool prev_was_sep = false;

    for (char c : stem) {
        bool is_allowed = std::isalnum(static_cast<unsigned char>(c)) ||
                         c == '-' || c == '_';

        if (is_allowed) {
            result += c;
            prev_was_sep = false;
        } else if (!prev_was_sep && !result.empty()) {
            result += '-';
            prev_was_sep = true;
        }
    }

    // Trim leading separators
    size_t start = 0;
    while (start < result.size() && (result[start] == '-' || result[start] == '_')) {
        ++start;
    }
    result = result.substr(start);

    // Trim trailing separators
    size_t end = result.size();
    while (end > 0 && (result[end - 1] == '-' || result[end - 1] == '_')) {
        --end;
    }
    result = result.substr(0, end);

    // Use "asset" if sanitization emptied the stem
    if (result.empty()) {
        return "asset";
    }

    return result;
}

} // namespace



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

AssetId generate_asset_id(const Project& project)
{
    std::set<int> used_numbers;

    // Extract numbers from existing IDs matching the "asset-N" format
    std::regex pattern("^asset-(\\d+)$");
    for (const auto& asset : project.assets) {
        std::smatch match;
        if (std::regex_match(asset.id, match, pattern)) {
            try {
                int num = std::stoi(match[1].str());
                used_numbers.insert(num);
            } catch (...) {
                // Ignore invalid numbers
            }
        }
    }

    // Find the next unused number starting from 1
    int next_number = 1;
    while (used_numbers.count(next_number)) {
        ++next_number;
    }

    return "asset-" + std::to_string(next_number);
}

Result<MediaAsset*> find_media_asset(
    Project& project,
    const AssetId& asset_id
)
{
    if (asset_id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Asset ID cannot be empty",
            std::nullopt
        };
    }

    for (auto& asset : project.assets) {
        if (asset.id == asset_id) {
            return &asset;
        }
    }

    return Error{
        ErrorCode::file_not_found,
        "Asset not found: " + asset_id,
        std::nullopt
    };
}

Result<const MediaAsset*> find_media_asset(
    const Project& project,
    const AssetId& asset_id
)
{
    if (asset_id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Asset ID cannot be empty",
            std::nullopt
        };
    }

    for (const auto& asset : project.assets) {
        if (asset.id == asset_id) {
            return &asset;
        }
    }

    return Error{
        ErrorCode::file_not_found,
        "Asset not found: " + asset_id,
        std::nullopt
    };
}

Result<MediaAssetType> detect_media_asset_type(
    const std::filesystem::path& source_path
)
{
    if (source_path.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Source path cannot be empty",
            std::nullopt
        };
    }

    std::error_code ec;
    if (!std::filesystem::exists(source_path, ec)) {
        if (ec) {
            return Error{
                ErrorCode::filesystem_error,
                "Failed to check if source exists: " + source_path.string(),
                ec.message()
            };
        }
        return Error{
            ErrorCode::file_not_found,
            "Source file not found: " + source_path.string(),
            std::nullopt
        };
    }

    if (!std::filesystem::is_regular_file(source_path, ec)) {
        if (ec) {
            return Error{
                ErrorCode::filesystem_error,
                "Failed to check if source is a regular file: " + source_path.string(),
                ec.message()
            };
        }
        return Error{
            ErrorCode::invalid_argument,
            "Source is not a regular file: " + source_path.string(),
            std::nullopt
        };
    }

    std::string ext = source_path.extension().string();

    if (is_audio_extension(ext)) {
        return MediaAssetType::audio;
    } else if (is_image_extension(ext)) {
        return MediaAssetType::image;
    } else if (is_video_extension(ext)) {
        return MediaAssetType::video;
    }

    return Error{
        ErrorCode::unsupported_format,
        "Unsupported media format: " + ext,
        std::nullopt
    };
}

Result<MediaAssetDestination> plan_media_asset_destination(
    const std::filesystem::path& project_root,
    const std::filesystem::path& source_path
)
{
    if (project_root.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Project root path cannot be empty",
            std::nullopt
        };
    }

    if (source_path.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Source path cannot be empty",
            std::nullopt
        };
    }

    std::error_code ec;
    if (!std::filesystem::exists(source_path, ec)) {
        if (ec) {
            return Error{
                ErrorCode::filesystem_error,
                "Failed to check if source exists: " + source_path.string(),
                ec.message()
            };
        }
        return Error{
            ErrorCode::file_not_found,
            "Source file not found: " + source_path.string(),
            std::nullopt
        };
    }

    if (!std::filesystem::is_regular_file(source_path, ec)) {
        if (ec) {
            return Error{
                ErrorCode::filesystem_error,
                "Failed to check if source is a regular file: " + source_path.string(),
                ec.message()
            };
        }
        return Error{
            ErrorCode::invalid_argument,
            "Source is not a regular file: " + source_path.string(),
            std::nullopt
        };
    }

    // Get the source stem and extension
    std::string stem = source_path.stem().string();
    std::string ext = to_lowercase(source_path.extension().string());

    // Sanitize the stem
    std::string sanitized = sanitize_stem(stem);

    // Build the destination directory
    auto media_dir = project_root / "media";

    // Build preferred filename
    std::string preferred_filename = sanitized + ext;

    // Check for collisions and find available filename
    std::string final_filename = preferred_filename;
    int suffix = 2;

    while (true) {
        auto candidate_path = media_dir / final_filename;

        if (!std::filesystem::exists(candidate_path, ec)) {
            if (ec) {
                return Error{
                    ErrorCode::filesystem_error,
                    "Failed to check for collision at: " + candidate_path.string(),
                    ec.message()
                };
            }
            // File doesn't exist, we can use this name
            break;
        }

        // File exists, try next suffix
        final_filename = sanitized + "-" + std::to_string(suffix) + ext;
        ++suffix;
    }

    // Build absolute path
    auto absolute_dest = media_dir / final_filename;

    // Build relative path
    auto relative_dest = std::filesystem::path("media") / final_filename;

    return MediaAssetDestination{
        absolute_dest,
        relative_dest
    };
}

} // namespace mvlab
