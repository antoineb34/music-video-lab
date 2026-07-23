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
