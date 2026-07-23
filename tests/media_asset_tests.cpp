#include <catch2/catch_all.hpp>
#include "media_asset.hpp"
#include <nlohmann/json.hpp>

TEST_CASE("MediaAssetType: to_string converts enum to string", "[media_asset]")
{
    CHECK(mvlab::to_string(mvlab::MediaAssetType::audio) == "audio");
    CHECK(mvlab::to_string(mvlab::MediaAssetType::image) == "image");
    CHECK(mvlab::to_string(mvlab::MediaAssetType::video) == "video");
}

TEST_CASE("MediaAssetType: parse_media_asset_type accepts valid types", "[media_asset]")
{
    {
        auto result = mvlab::parse_media_asset_type("audio");
        REQUIRE(result.has_value());
        CHECK(result.value() == mvlab::MediaAssetType::audio);
    }

    {
        auto result = mvlab::parse_media_asset_type("image");
        REQUIRE(result.has_value());
        CHECK(result.value() == mvlab::MediaAssetType::image);
    }

    {
        auto result = mvlab::parse_media_asset_type("video");
        REQUIRE(result.has_value());
        CHECK(result.value() == mvlab::MediaAssetType::video);
    }
}

TEST_CASE("MediaAssetType: parse_media_asset_type rejects unknown type", "[media_asset]")
{
    auto result = mvlab::parse_media_asset_type("unknown");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(result.error().message.find("Unknown media type") != std::string::npos);
}

TEST_CASE("MediaAsset: validate_media_asset accepts valid asset", "[media_asset]")
{
    mvlab::MediaAsset asset{
        "asset-1",
        mvlab::MediaAssetType::audio,
        "My Audio Track",
        "media/audio.wav",
        1024000
    };

    auto result = mvlab::validate_media_asset(asset);

    REQUIRE(result.has_value());
}

TEST_CASE("MediaAsset: validate_media_asset rejects empty ID", "[media_asset]")
{
    mvlab::MediaAsset asset{
        "",
        mvlab::MediaAssetType::audio,
        "My Audio Track",
        "media/audio.wav",
        1024000
    };

    auto result = mvlab::validate_media_asset(asset);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(result.error().message.find("ID cannot be empty") != std::string::npos);
}

TEST_CASE("MediaAsset: validate_media_asset rejects empty display name", "[media_asset]")
{
    mvlab::MediaAsset asset{
        "asset-1",
        mvlab::MediaAssetType::audio,
        "",
        "media/audio.wav",
        1024000
    };

    auto result = mvlab::validate_media_asset(asset);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(result.error().message.find("display name cannot be empty") != std::string::npos);
}

TEST_CASE("MediaAsset: validate_media_asset rejects empty path", "[media_asset]")
{
    mvlab::MediaAsset asset{
        "asset-1",
        mvlab::MediaAssetType::audio,
        "My Audio Track",
        "",
        1024000
    };

    auto result = mvlab::validate_media_asset(asset);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(result.error().message.find("relative path cannot be empty") != std::string::npos);
}

TEST_CASE("MediaAsset: validate_media_asset rejects absolute path", "[media_asset]")
{
    mvlab::MediaAsset asset{
        "asset-1",
        mvlab::MediaAssetType::audio,
        "My Audio Track",
        "/absolute/path/to/audio.wav",
        1024000
    };

    auto result = mvlab::validate_media_asset(asset);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(result.error().message.find("must be relative") != std::string::npos);
}

TEST_CASE("MediaAsset: validate_media_asset rejects path with .. traversal", "[media_asset]")
{
    mvlab::MediaAsset asset{
        "asset-1",
        mvlab::MediaAssetType::audio,
        "My Audio Track",
        "media/../../../etc/passwd",
        1024000
    };

    auto result = mvlab::validate_media_asset(asset);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(result.error().message.find("..") != std::string::npos);
}

TEST_CASE("MediaAsset: validate_media_asset rejects zero file size", "[media_asset]")
{
    mvlab::MediaAsset asset{
        "asset-1",
        mvlab::MediaAssetType::audio,
        "My Audio Track",
        "media/audio.wav",
        0
    };

    auto result = mvlab::validate_media_asset(asset);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(result.error().message.find("file size must not be zero") != std::string::npos);
}

TEST_CASE("MediaAsset: JSON serialization round trip", "[media_asset]")
{
    mvlab::MediaAsset original{
        "asset-123",
        mvlab::MediaAssetType::video,
        "Background Video",
        "media/videos/bg.mp4",
        50000000
    };

    nlohmann::json j = original;
    mvlab::MediaAsset deserialized = j;

    CHECK(deserialized.id == original.id);
    CHECK(deserialized.type == original.type);
    CHECK(deserialized.display_name == original.display_name);
    CHECK(deserialized.relative_path == original.relative_path);
    CHECK(deserialized.file_size == original.file_size);
}

TEST_CASE("MediaAsset: JSON deserialization missing required field", "[media_asset]")
{
    nlohmann::json j = nlohmann::json{
        {"id", "asset-1"},
        {"type", "audio"},
        {"display_name", "My Track"}
    };

    REQUIRE_THROWS_AS(
        j.get<mvlab::MediaAsset>(),
        nlohmann::json::out_of_range
    );
}

TEST_CASE("MediaAsset: JSON deserialization invalid field type", "[media_asset]")
{
    nlohmann::json j = nlohmann::json{
        {"id", "asset-1"},
        {"type", "audio"},
        {"display_name", "My Track"},
        {"relative_path", "media/audio.wav"},
        {"file_size", "not a number"}
    };

    REQUIRE_THROWS_AS(
        j.get<mvlab::MediaAsset>(),
        nlohmann::json::type_error
    );
}

TEST_CASE("MediaAsset: JSON deserialization unknown media type", "[media_asset]")
{
    nlohmann::json j = nlohmann::json{
        {"id", "asset-1"},
        {"type", "unknown"},
        {"display_name", "My Track"},
        {"relative_path", "media/audio.wav"},
        {"file_size", 1024000}
    };

    REQUIRE_THROWS(j.get<mvlab::MediaAsset>());
}

TEST_CASE("MediaAsset: JSON serialization preserves all fields", "[media_asset]")
{
    mvlab::MediaAsset asset{
        "asset-456",
        mvlab::MediaAssetType::image,
        "Cover Art",
        "media/images/cover.png",
        256000
    };

    nlohmann::json j = asset;

    CHECK(j["id"] == "asset-456");
    CHECK(j["type"] == "image");
    CHECK(j["display_name"] == "Cover Art");
    CHECK(j["relative_path"] == "media/images/cover.png");
    CHECK(j["file_size"] == 256000);
}
