#include <catch2/catch_all.hpp>
#include "media_asset.hpp"
#include "project_model.hpp"
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

TEST_CASE("Asset ID generation: empty project generates asset-1", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();

    auto generated = mvlab::generate_asset_id(project);

    CHECK(generated == "asset-1");
}

TEST_CASE("Asset ID generation: existing asset-1 generates asset-2", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Track", "media/track.mp3", 1000000
    });

    auto generated = mvlab::generate_asset_id(project);

    CHECK(generated == "asset-2");
}

TEST_CASE("Asset ID generation: sequential IDs generate next ID", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Track1", "media/track1.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-2", mvlab::MediaAssetType::audio, "Track2", "media/track2.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-3", mvlab::MediaAssetType::audio, "Track3", "media/track3.mp3", 1000000
    });

    auto generated = mvlab::generate_asset_id(project);

    CHECK(generated == "asset-4");
}

TEST_CASE("Asset ID generation: gaps do not cause collisions", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Track1", "media/track1.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-3", mvlab::MediaAssetType::audio, "Track3", "media/track3.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-5", mvlab::MediaAssetType::audio, "Track5", "media/track5.mp3", 1000000
    });

    auto generated = mvlab::generate_asset_id(project);

    CHECK(generated == "asset-2");
}

TEST_CASE("Asset ID generation: unrelated custom IDs do not break generation", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "custom-id", mvlab::MediaAssetType::audio, "Track", "media/track.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "user-defined-123", mvlab::MediaAssetType::audio, "Track2", "media/track2.mp3", 1000000
    });

    auto generated = mvlab::generate_asset_id(project);

    CHECK(generated == "asset-1");
}

TEST_CASE("Asset ID generation: malformed managed-style IDs do not crash", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-abc", mvlab::MediaAssetType::audio, "Bad1", "media/track1.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-", mvlab::MediaAssetType::audio, "Bad2", "media/track2.mp3", 1000000
    });

    auto generated = mvlab::generate_asset_id(project);

    CHECK(generated == "asset-1");
}

TEST_CASE("Asset ID generation: generated IDs pass validation", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();

    auto generated = mvlab::generate_asset_id(project);

    mvlab::MediaAsset test_asset{
        generated,
        mvlab::MediaAssetType::audio,
        "Test",
        "media/test.mp3",
        1000000
    };

    auto validation = mvlab::validate_media_asset(test_asset);
    REQUIRE(validation.has_value());
}

TEST_CASE("Asset ID generation: generated ID is unique across project", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Track1", "media/track1.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-2", mvlab::MediaAssetType::audio, "Track2", "media/track2.mp3", 1000000
    });

    auto generated = mvlab::generate_asset_id(project);

    for (const auto& asset : project.assets) {
        CHECK(asset.id != generated);
    }
}

TEST_CASE("Asset lookup: mutable lookup finds existing asset", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Track", "media/track.mp3", 1000000
    });

    auto result = mvlab::find_media_asset(project, "asset-1");

    REQUIRE(result.has_value());
    CHECK(result.value()->id == "asset-1");
    CHECK(result.value()->display_name == "Track");
}

TEST_CASE("Asset lookup: const lookup finds existing asset", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Track", "media/track.mp3", 1000000
    });

    const auto& const_project = project;
    auto result = mvlab::find_media_asset(const_project, "asset-1");

    REQUIRE(result.has_value());
    CHECK(result.value()->id == "asset-1");
}

TEST_CASE("Asset lookup: mutable lookup permits changing the asset", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Original", "media/track.mp3", 1000000
    });

    auto result = mvlab::find_media_asset(project, "asset-1");
    REQUIRE(result.has_value());

    result.value()->display_name = "Modified";

    CHECK(project.assets[0].display_name == "Modified");
}

TEST_CASE("Asset lookup: const lookup does not permit mutation", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Track", "media/track.mp3", 1000000
    });

    const auto& const_project = project;
    auto result = mvlab::find_media_asset(const_project, "asset-1");

    REQUIRE(result.has_value());

    // This would not compile: result.value()->display_name = "Modified";
    CHECK(result.value()->display_name == "Track");
}

TEST_CASE("Asset lookup: empty requested ID fails", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Track", "media/track.mp3", 1000000
    });

    auto result = mvlab::find_media_asset(project, "");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(result.error().message.find("cannot be empty") != std::string::npos);
}

TEST_CASE("Asset lookup: unknown ID fails", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Track", "media/track.mp3", 1000000
    });

    auto result = mvlab::find_media_asset(project, "asset-999");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::file_not_found);
    CHECK(result.error().message.find("Asset not found") != std::string::npos);
}

TEST_CASE("Asset lookup: error code and details are correct", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();

    auto result = mvlab::find_media_asset(project, "unknown-asset");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::file_not_found);
    CHECK(result.error().message.find("unknown-asset") != std::string::npos);
}

TEST_CASE("Asset lookup: mutable and const versions find same asset", "[media_asset]")
{
    auto project = mvlab::create_project("Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::video, "Background", "media/bg.mp4", 50000000
    });

    auto mutable_result = mvlab::find_media_asset(project, "asset-1");
    const auto& const_project = project;
    auto const_result = mvlab::find_media_asset(const_project, "asset-1");

    REQUIRE(mutable_result.has_value());
    REQUIRE(const_result.has_value());
    CHECK(mutable_result.value()->id == const_result.value()->id);
    CHECK(mutable_result.value()->display_name == const_result.value()->display_name);
    CHECK(mutable_result.value()->file_size == const_result.value()->file_size);
}
