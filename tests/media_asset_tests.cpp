#include <catch2/catch_all.hpp>
#include "media_asset.hpp"
#include "project_model.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

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

// Helper to create a temporary test file
std::filesystem::path create_temp_file(const std::string& suffix)
{
    auto temp_dir = std::filesystem::temp_directory_path();
    std::string template_str = temp_dir.string() + "/mvlab_test_XXXXXX" + suffix;

    // Create a unique temporary file by mkstemp-like approach
    static int counter = 0;
    std::string filename = temp_dir.string() + "/mvlab_test_" +
                          std::to_string(++counter) + suffix;
    std::filesystem::path temp_path{filename};

    // Create the file
    std::ofstream file(temp_path);
    file << "test content";
    file.close();

    return temp_path;
}

// Media type detection: audio extensions
TEST_CASE("Media type detection: mp3 extension", "[media_asset]")
{
    auto temp = create_temp_file(".mp3");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::audio);
}

TEST_CASE("Media type detection: wav extension", "[media_asset]")
{
    auto temp = create_temp_file(".wav");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::audio);
}

TEST_CASE("Media type detection: flac extension", "[media_asset]")
{
    auto temp = create_temp_file(".flac");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::audio);
}

TEST_CASE("Media type detection: ogg extension", "[media_asset]")
{
    auto temp = create_temp_file(".ogg");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::audio);
}

TEST_CASE("Media type detection: m4a extension", "[media_asset]")
{
    auto temp = create_temp_file(".m4a");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::audio);
}

TEST_CASE("Media type detection: aac extension", "[media_asset]")
{
    auto temp = create_temp_file(".aac");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::audio);
}

// Media type detection: image extensions
TEST_CASE("Media type detection: png extension", "[media_asset]")
{
    auto temp = create_temp_file(".png");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::image);
}

TEST_CASE("Media type detection: jpg extension", "[media_asset]")
{
    auto temp = create_temp_file(".jpg");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::image);
}

TEST_CASE("Media type detection: jpeg extension", "[media_asset]")
{
    auto temp = create_temp_file(".jpeg");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::image);
}

TEST_CASE("Media type detection: webp extension", "[media_asset]")
{
    auto temp = create_temp_file(".webp");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::image);
}

TEST_CASE("Media type detection: bmp extension", "[media_asset]")
{
    auto temp = create_temp_file(".bmp");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::image);
}

// Media type detection: video extensions
TEST_CASE("Media type detection: mp4 extension", "[media_asset]")
{
    auto temp = create_temp_file(".mp4");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::video);
}

TEST_CASE("Media type detection: mkv extension", "[media_asset]")
{
    auto temp = create_temp_file(".mkv");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::video);
}

TEST_CASE("Media type detection: mov extension", "[media_asset]")
{
    auto temp = create_temp_file(".mov");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::video);
}

TEST_CASE("Media type detection: webm extension", "[media_asset]")
{
    auto temp = create_temp_file(".webm");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::video);
}

TEST_CASE("Media type detection: avi extension", "[media_asset]")
{
    auto temp = create_temp_file(".avi");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::video);
}

// Media type detection: case-insensitive
TEST_CASE("Media type detection: uppercase extension", "[media_asset]")
{
    auto temp = create_temp_file(".MP3");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::audio);
}

TEST_CASE("Media type detection: mixed-case extension", "[media_asset]")
{
    auto temp = create_temp_file(".Mp4");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    CHECK(result.value() == mvlab::MediaAssetType::video);
}

// Media type detection: error cases
TEST_CASE("Media type detection: unsupported extension", "[media_asset]")
{
    auto temp = create_temp_file(".txt");
    auto result = mvlab::detect_media_asset_type(temp);
    std::filesystem::remove(temp);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::unsupported_format);
}

TEST_CASE("Media type detection: no extension", "[media_asset]")
{
    auto temp_path = std::filesystem::temp_directory_path() / "mvlab_test_no_ext";
    std::ofstream file(temp_path);
    file << "test";
    file.close();

    auto result = mvlab::detect_media_asset_type(temp_path);
    std::filesystem::remove(temp_path);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::unsupported_format);
}

TEST_CASE("Media type detection: missing file", "[media_asset]")
{
    std::filesystem::path nonexistent = "/tmp/mvlab_nonexistent_12345.mp3";
    auto result = mvlab::detect_media_asset_type(nonexistent);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::file_not_found);
}

TEST_CASE("Media type detection: directory instead of file", "[media_asset]")
{
    auto temp_dir = std::filesystem::temp_directory_path() / "mvlab_test_dir";
    std::filesystem::create_directory(temp_dir);

    auto result = mvlab::detect_media_asset_type(temp_dir);
    std::filesystem::remove(temp_dir);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
}

TEST_CASE("Media type detection: empty path", "[media_asset]")
{
    auto result = mvlab::detect_media_asset_type("");

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
}

// Destination planning
TEST_CASE("Media asset destination: normal filename with lowercase extension", "[media_asset]")
{
    auto temp = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_proj_1";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto result = mvlab::plan_media_asset_destination(project_root, temp);
    std::filesystem::remove_all(project_root);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    auto dest = result.value();

    CHECK(dest.relative_path.string().find("media/") == 0);
    CHECK(dest.relative_path.string().find("..") == std::string::npos);
}

TEST_CASE("Media asset destination: uppercase extension normalized to lowercase", "[media_asset]")
{
    auto temp = create_temp_file(".MP3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_proj_2";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto result = mvlab::plan_media_asset_destination(project_root, temp);
    std::filesystem::remove_all(project_root);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    auto dest = result.value();

    // Check that extension in destination is lowercase
    CHECK(dest.relative_path.string().find(".mp3") != std::string::npos);
    CHECK(dest.relative_path.string().find(".MP3") == std::string::npos);
}

TEST_CASE("Media asset destination: spaces converted to dashes", "[media_asset]")
{
    // Create file with spaces in name
    auto temp_dir = std::filesystem::temp_directory_path();
    auto temp_path = temp_dir / "my song file.mp3";
    std::ofstream file(temp_path);
    file << "test";
    file.close();

    auto project_root = std::filesystem::temp_directory_path() / "mvlab_proj_3";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto result = mvlab::plan_media_asset_destination(project_root, temp_path);
    std::filesystem::remove_all(project_root);
    std::filesystem::remove(temp_path);

    REQUIRE(result.has_value());
    auto dest = result.value();

    // Spaces should be converted to dashes
    CHECK(dest.relative_path.string().find("my-song-file") != std::string::npos);
    CHECK(dest.relative_path.string().find("my song file") == std::string::npos);
}

TEST_CASE("Media asset destination: punctuation collapsed", "[media_asset]")
{
    // Create file with punctuation
    auto temp_dir = std::filesystem::temp_directory_path();
    auto temp_path = temp_dir / "song!!!cool...mp3";
    std::ofstream file(temp_path);
    file << "test";
    file.close();

    auto project_root = std::filesystem::temp_directory_path() / "mvlab_proj_4";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto result = mvlab::plan_media_asset_destination(project_root, temp_path);
    std::filesystem::remove_all(project_root);
    std::filesystem::remove(temp_path);

    REQUIRE(result.has_value());
    auto dest = result.value();

    // Runs of unsupported characters should collapse to single dash
    CHECK(dest.relative_path.string().find("song-cool") != std::string::npos);
}

TEST_CASE("Media asset destination: unicode stem falls back to asset", "[media_asset]")
{
    // Create file with pure unicode name (no ASCII letters/digits)
    auto temp_dir = std::filesystem::temp_directory_path();
    auto temp_path = temp_dir / "🎵🎶.mp3";
    std::ofstream file(temp_path);
    file << "test";
    file.close();

    auto project_root = std::filesystem::temp_directory_path() / "mvlab_proj_5";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto result = mvlab::plan_media_asset_destination(project_root, temp_path);
    std::filesystem::remove_all(project_root);
    std::filesystem::remove(temp_path);

    REQUIRE(result.has_value());
    auto dest = result.value();

    // Should use "asset" as fallback
    CHECK(dest.relative_path.string().find("asset.mp3") != std::string::npos);
}

TEST_CASE("Media asset destination: collision produces -2 suffix", "[media_asset]")
{
    auto temp = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_proj_6";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    // Create the preferred file
    auto preferred = project_root / "media" / (std::filesystem::path(temp).stem().string() + ".mp3");
    std::ofstream file(preferred);
    file << "existing";
    file.close();

    auto result = mvlab::plan_media_asset_destination(project_root, temp);
    std::filesystem::remove_all(project_root);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    auto dest = result.value();

    // Should have -2 suffix
    CHECK(dest.relative_path.string().find("-2.mp3") != std::string::npos);
}

TEST_CASE("Media asset destination: existing -2 produces -3 suffix", "[media_asset]")
{
    auto temp = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_proj_7";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    // Create both preferred and -2
    auto stem = std::filesystem::path(temp).stem().string();
    auto preferred = project_root / "media" / (stem + ".mp3");
    auto suffix2 = project_root / "media" / (stem + "-2.mp3");

    std::ofstream file1(preferred);
    file1 << "existing1";
    file1.close();

    std::ofstream file2(suffix2);
    file2 << "existing2";
    file2.close();

    auto result = mvlab::plan_media_asset_destination(project_root, temp);
    std::filesystem::remove_all(project_root);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    auto dest = result.value();

    // Should have -3 suffix
    CHECK(dest.relative_path.string().find("-3.mp3") != std::string::npos);
}

TEST_CASE("Media asset destination: relative path starts with media/", "[media_asset]")
{
    auto temp = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_proj_8";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto result = mvlab::plan_media_asset_destination(project_root, temp);
    std::filesystem::remove_all(project_root);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    auto dest = result.value();

    CHECK(dest.relative_path.string().find("media/") == 0);
}

TEST_CASE("Media asset destination: relative path has no ..", "[media_asset]")
{
    auto temp = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_proj_9";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto result = mvlab::plan_media_asset_destination(project_root, temp);
    std::filesystem::remove_all(project_root);
    std::filesystem::remove(temp);

    REQUIRE(result.has_value());
    auto dest = result.value();

    CHECK(dest.relative_path.string().find("..") == std::string::npos);
}

TEST_CASE("Media asset destination: empty project root", "[media_asset]")
{
    auto temp = create_temp_file(".mp3");
    auto result = mvlab::plan_media_asset_destination("", temp);
    std::filesystem::remove(temp);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
}

TEST_CASE("Media asset destination: empty source path", "[media_asset]")
{
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_proj_10";
    std::filesystem::create_directory(project_root);

    auto result = mvlab::plan_media_asset_destination(project_root, "");
    std::filesystem::remove_all(project_root);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
}

// Media asset import: successful imports
TEST_CASE("Media asset import: successful audio import", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_1";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);

    REQUIRE(result.has_value());
    auto imported = result.value();
    CHECK(imported.type == mvlab::MediaAssetType::audio);
    CHECK(!imported.id.empty());
    CHECK(imported.display_name.find(".mp3") != std::string::npos);
    CHECK(imported.relative_path.string().find("media/") == 0);
    CHECK(imported.file_size > 0);
}

TEST_CASE("Media asset import: successful image import", "[media_asset]")
{
    auto source = create_temp_file(".png");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_2";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);

    REQUIRE(result.has_value());
    CHECK(result.value().type == mvlab::MediaAssetType::image);
}

TEST_CASE("Media asset import: successful video import", "[media_asset]")
{
    auto source = create_temp_file(".mp4");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_3";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);

    REQUIRE(result.has_value());
    CHECK(result.value().type == mvlab::MediaAssetType::video);
}

TEST_CASE("Media asset import: copied file exists in media directory", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_4";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source);

    REQUIRE(result.has_value());
    auto imported = result.value();

    auto expected_path = project_root / imported.relative_path;
    CHECK(std::filesystem::exists(expected_path));
    CHECK(std::filesystem::is_regular_file(expected_path));

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);
}

TEST_CASE("Media asset import: copied bytes match source bytes", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto source_size = std::filesystem::file_size(source);

    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_5";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source);

    REQUIRE(result.has_value());
    auto imported = result.value();
    CHECK(imported.file_size == source_size);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);
}

TEST_CASE("Media asset import: asset appended to project", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_6";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    size_t original_count = project.assets.size();

    auto result = mvlab::import_media_asset(project, project_root, source);

    REQUIRE(result.has_value());
    CHECK(project.assets.size() == original_count + 1);
    CHECK(project.assets.back().id == result.value().id);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);
}

TEST_CASE("Media asset import: project.json saved with new asset", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_7";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source);

    REQUIRE(result.has_value());
    auto imported_id = result.value().id;

    auto project_file = project_root / "project.json";
    REQUIRE(std::filesystem::exists(project_file));

    auto loaded = mvlab::load_project(project_file.string());
    REQUIRE(loaded.has_value());
    CHECK(loaded.value().assets.size() == 1);
    CHECK(loaded.value().assets[0].id == imported_id);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);
}

TEST_CASE("Media asset import: uppercase extension normalized to lowercase", "[media_asset]")
{
    auto source = create_temp_file(".MP3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_8";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source);

    REQUIRE(result.has_value());
    auto imported = result.value();

    // Check that the managed path has lowercase extension
    CHECK(imported.relative_path.string().find(".mp3") != std::string::npos);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);
}

TEST_CASE("Media asset import: filename sanitization respected", "[media_asset]")
{
    auto temp_dir = std::filesystem::temp_directory_path();
    auto source_path = temp_dir / "my  song!!!file.mp3";
    std::ofstream file(source_path);
    file << "test";
    file.close();

    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_9";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source_path);

    REQUIRE(result.has_value());
    auto imported = result.value();

    // Should have sanitized the name
    CHECK(imported.relative_path.string().find("my-song-file") != std::string::npos);

    std::filesystem::remove(source_path);
    std::filesystem::remove_all(project_root);
}

TEST_CASE("Media asset import: destination collision produces -2", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_10";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    // Create existing file with the preferred name
    auto stem = std::filesystem::path(source).stem().string();
    auto existing = project_root / "media" / (stem + ".mp3");
    std::ofstream f(existing);
    f << "existing";
    f.close();

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source);

    REQUIRE(result.has_value());
    auto imported = result.value();

    // Should have -2 suffix
    CHECK(imported.relative_path.string().find("-2.mp3") != std::string::npos);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);
}

TEST_CASE("Media asset import: second import receives next asset ID", "[media_asset]")
{
    auto source1 = create_temp_file(".mp3");
    auto source2 = create_temp_file(".png");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_11";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();

    auto result1 = mvlab::import_media_asset(project, project_root, source1);
    REQUIRE(result1.has_value());
    auto id1 = result1.value().id;

    auto result2 = mvlab::import_media_asset(project, project_root, source2);
    REQUIRE(result2.has_value());
    auto id2 = result2.value().id;

    CHECK(id1 != id2);
    CHECK(project.assets.size() == 2);

    std::filesystem::remove(source1);
    std::filesystem::remove(source2);
    std::filesystem::remove_all(project_root);
}

TEST_CASE("Media asset import: creates missing media directory", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_12";
    std::filesystem::create_directory(project_root);
    // Do NOT create media/ directory

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source);

    REQUIRE(result.has_value());
    CHECK(std::filesystem::exists(project_root / "media"));
    CHECK(std::filesystem::is_directory(project_root / "media"));

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);
}

// Media asset import: failure and rollback
TEST_CASE("Media asset import: empty project root rejected", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project = mvlab::create_project("Test").value();

    auto result = mvlab::import_media_asset(project, "", source);
    std::filesystem::remove(source);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
}

TEST_CASE("Media asset import: missing project root rejected", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project = mvlab::create_project("Test").value();
    auto nonexistent = std::filesystem::temp_directory_path() / "mvlab_nonexistent_proj";

    auto result = mvlab::import_media_asset(project, nonexistent, source);
    std::filesystem::remove(source);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::file_not_found);
}

TEST_CASE("Media asset import: project root as regular file rejected", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project_file = std::filesystem::temp_directory_path() / "mvlab_not_a_dir";
    std::ofstream f(project_file);
    f << "not a directory";
    f.close();

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_file, source);

    std::filesystem::remove(source);
    std::filesystem::remove(project_file);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
}

TEST_CASE("Media asset import: empty source path rejected", "[media_asset]")
{
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_13";
    std::filesystem::create_directory(project_root);

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, "");

    std::filesystem::remove_all(project_root);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
}

TEST_CASE("Media asset import: missing source rejected", "[media_asset]")
{
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_14";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto nonexistent = std::filesystem::temp_directory_path() / "mvlab_nonexistent.mp3";

    auto result = mvlab::import_media_asset(project, project_root, nonexistent);

    std::filesystem::remove_all(project_root);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::file_not_found);
}

TEST_CASE("Media asset import: source as directory rejected", "[media_asset]")
{
    auto source_dir = std::filesystem::temp_directory_path() / "mvlab_source_dir";
    std::filesystem::create_directory(source_dir);

    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_15";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source_dir);

    std::filesystem::remove_all(source_dir);
    std::filesystem::remove_all(project_root);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::invalid_argument);
}

TEST_CASE("Media asset import: unsupported extension rejected", "[media_asset]")
{
    auto source = create_temp_file(".txt");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_16";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    auto result = mvlab::import_media_asset(project, project_root, source);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == mvlab::ErrorCode::unsupported_format);
}

TEST_CASE("Media asset import: rollback removes copied file on save failure", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_17";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    // Create an invalid project (mismatched export settings)
    auto project = mvlab::create_project("Test").value();
    project.export_settings.width = 0;  // Invalid

    auto result = mvlab::import_media_asset(project, project_root, source);

    // Should fail on validation
    REQUIRE_FALSE(result.has_value());

    // File should be removed
    auto media_dir = project_root / "media";
    bool media_empty = true;
    if (std::filesystem::exists(media_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(media_dir)) {
            if (std::filesystem::is_regular_file(entry)) {
                media_empty = false;
                break;
            }
        }
    }
    CHECK(media_empty);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);
}

TEST_CASE("Media asset import: rollback restores project state on failure", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_18";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    size_t original_asset_count = project.assets.size();

    // Create an invalid project for rollback
    project.export_settings.width = 0;

    auto result = mvlab::import_media_asset(project, project_root, source);

    REQUIRE_FALSE(result.has_value());
    // Asset count should not change
    CHECK(project.assets.size() == original_asset_count);

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);
}

TEST_CASE("Media asset import: no asset remains after rollback", "[media_asset]")
{
    auto source = create_temp_file(".mp3");
    auto project_root = std::filesystem::temp_directory_path() / "mvlab_import_19";
    std::filesystem::create_directory(project_root);
    std::filesystem::create_directory(project_root / "media");

    auto project = mvlab::create_project("Test").value();
    project.export_settings.width = 0;  // Make it invalid

    auto result = mvlab::import_media_asset(project, project_root, source);

    REQUIRE_FALSE(result.has_value());
    CHECK(project.assets.empty());

    std::filesystem::remove(source);
    std::filesystem::remove_all(project_root);
}
