#include <catch2/catch_all.hpp>
#include "project_model.hpp"
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>

namespace fs = std::filesystem;

TEST_CASE("Project model: create_project creates default project", "[project_model]")
{
    auto outcome = mvlab::create_project("Test Project");

    REQUIRE(outcome.has_value());
    const auto& project = outcome.value();
    REQUIRE(project.schema_version == 2);
    REQUIRE(project.name == "Test Project");
    REQUIRE(!project.audio.path.has_value());
    REQUIRE(!project.background.path.has_value());
    REQUIRE(!project.background.type.has_value());
    REQUIRE(!project.lyrics.path.has_value());
    REQUIRE(!project.lyrics.format.has_value());
    REQUIRE(project.export_settings.width == 1920);
    REQUIRE(project.export_settings.height == 1080);
    REQUIRE(project.export_settings.fps == 30);
    REQUIRE(project.timeline.tracks.empty());
}

TEST_CASE("Project model: create_project rejects empty name", "[project_model]")
{
    auto outcome = mvlab::create_project("");

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
}

TEST_CASE("Project model: validate_project accepts valid project", "[project_model]")
{
    auto project = mvlab::create_project("Test Project").value();
    auto outcome = mvlab::validate_project(project);

    REQUIRE(outcome.has_value());
}

TEST_CASE("Project model: validate_project rejects empty name", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();
    project.name = "";

    auto outcome = mvlab::validate_project(project);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(outcome.error().message.find("name cannot be empty") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects invalid schema version", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();
    project.schema_version = 999;

    auto outcome = mvlab::validate_project(project);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::unsupported_schema);
    CHECK(outcome.error().message.find("Unsupported schema version") != std::string::npos);
}

TEST_CASE("Project model: validate_project accepts legacy schema version 1", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();
    project.schema_version = 1;

    auto outcome = mvlab::validate_project(project);

    REQUIRE(outcome.has_value());
}

TEST_CASE("Project model: validate_project rejects zero width", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();
    project.export_settings.width = 0;

    auto outcome = mvlab::validate_project(project);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(outcome.error().message.find("width must be greater than zero") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects zero height", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();
    project.export_settings.height = 0;

    auto outcome = mvlab::validate_project(project);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(outcome.error().message.find("height must be greater than zero") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects zero fps", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();
    project.export_settings.fps = 0;

    auto outcome = mvlab::validate_project(project);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(outcome.error().message.find("fps must be greater than zero") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects invalid background type", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();
    project.background.type = "invalid";

    auto outcome = mvlab::validate_project(project);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(outcome.error().message.find("Invalid background type") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects invalid lyrics format", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();
    project.lyrics.format = "invalid";

    auto outcome = mvlab::validate_project(project);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(outcome.error().message.find("Invalid lyrics format") != std::string::npos);
}

TEST_CASE("Project model: validate_project accepts valid background types", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();

    project.background.type = "image";
    CHECK(mvlab::validate_project(project).has_value());

    project.background.type = "video";
    CHECK(mvlab::validate_project(project).has_value());
}

TEST_CASE("Project model: validate_project accepts valid lyrics formats", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();

    project.lyrics.format = "txt";
    CHECK(mvlab::validate_project(project).has_value());

    project.lyrics.format = "lrc";
    CHECK(mvlab::validate_project(project).has_value());
}

TEST_CASE("Project model: save_project and load_project round trip", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "round_trip";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto original = mvlab::create_project("Round Trip Test").value();
    original.audio.path = "/path/to/audio.mp3";
    original.background.path = "/path/to/bg.mp4";
    original.background.type = "video";
    original.export_settings.width = 1280;
    original.export_settings.height = 720;

    auto save_outcome = mvlab::save_project(original, file_path);
    REQUIRE(save_outcome.has_value());

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    const auto& loaded = load_outcome.value();
    REQUIRE(loaded.name == "Round Trip Test");
    REQUIRE(loaded.audio.path == "/path/to/audio.mp3");
    REQUIRE(loaded.background.path == "/path/to/bg.mp4");
    REQUIRE(loaded.background.type == "video");
    REQUIRE(loaded.export_settings.width == 1280);
    REQUIRE(loaded.export_settings.height == 720);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: load_project handles missing file", "[project_model]")
{
    auto outcome = mvlab::load_project("/nonexistent/path/project.json");

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::file_not_found);
    CHECK(outcome.error().message.find("not found") != std::string::npos);
}

TEST_CASE("Project model: load_project handles malformed JSON", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "malformed";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    std::ofstream file(file_path);
    file << "{ invalid json }";
    file.close();

    auto outcome = mvlab::load_project(file_path);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::malformed_project);
    REQUIRE(outcome.error().details.has_value());

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: load_project rejects unsupported schema version", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "unsupported_schema";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 999;
    j["name"] = "Test";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 1920}, {"height", 1080}, {"fps", 30}};

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto outcome = mvlab::load_project(file_path);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::unsupported_schema);
    CHECK(outcome.error().message.find("Unsupported schema version") != std::string::npos);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: load_project reports malformed_project for invalid field data", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "invalid_field";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 1;
    j["name"] = "Test";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 0}, {"height", 1080}, {"fps", 30}};

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto outcome = mvlab::load_project(file_path);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::malformed_project);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: save_project creates parent directories", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "nested" / "path" / "to" / "project";
    fs::remove_all(temp_dir.parent_path());

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Nested Test").value();
    auto outcome = mvlab::save_project(project, file_path);

    REQUIRE(outcome.has_value());
    REQUIRE(fs::exists(file_path));

    fs::remove_all(temp_dir.parent_path());
}

TEST_CASE("Project model: path containing spaces", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "path with spaces";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Spaces Test").value();
    auto save_outcome = mvlab::save_project(project, file_path);

    REQUIRE(save_outcome.has_value());
    REQUIRE(fs::exists(file_path));

    auto load_outcome = mvlab::load_project(file_path);

    REQUIRE(load_outcome.has_value());
    REQUIRE(load_outcome.value().name == "Spaces Test");

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: validate_project accepts null optional fields", "[project_model]")
{
    auto project = mvlab::create_project("Test").value();

    project.audio.path.reset();
    project.audio.analysis.reset();
    project.background.path.reset();
    project.background.type.reset();
    project.lyrics.path.reset();
    project.lyrics.format.reset();

    auto outcome = mvlab::validate_project(project);

    REQUIRE(outcome.has_value());
}

TEST_CASE("Project model: create_project_on_disk creates the .mvlab folder structure", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "on_disk";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    auto outcome = mvlab::create_project_on_disk(temp_dir / "myproject", "My Project");

    REQUIRE(outcome.has_value());
    const auto& paths = outcome.value();
    CHECK(paths.root.string().ends_with(".mvlab"));
    CHECK(fs::is_directory(paths.root / "media"));
    CHECK(fs::is_directory(paths.root / "lyrics"));
    CHECK(fs::is_directory(paths.root / "analysis"));
    CHECK(fs::is_directory(paths.root / "renders"));
    CHECK(fs::exists(paths.project_file));

    auto load_outcome = mvlab::load_project(paths.project_file.string());
    REQUIRE(load_outcome.has_value());
    CHECK(load_outcome.value().name == "My Project");

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: create_project_on_disk refuses to overwrite an existing non-empty project", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "overwrite_refusal";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    auto first = mvlab::create_project_on_disk(temp_dir / "myproject", "First");
    REQUIRE(first.has_value());

    auto second = mvlab::create_project_on_disk(temp_dir / "myproject", "Second");
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(second.error().message.find("already exists") != std::string::npos);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: create_project_on_disk accepts a path already ending in .mvlab", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "explicit_suffix";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    auto outcome = mvlab::create_project_on_disk(temp_dir / "explicit.mvlab", "Explicit");

    REQUIRE(outcome.has_value());
    CHECK(outcome.value().root == temp_dir / "explicit.mvlab");

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: load_project reports permission_denied for an unreadable file", "[project_model]")
{
    if (getuid() == 0) {
        return; // root bypasses POSIX permission checks; skip under CI running as root.
    }

    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "unreadable";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();
    auto project = mvlab::create_project("Unreadable").value();
    REQUIRE(mvlab::save_project(project, file_path).has_value());

    chmod(file_path.c_str(), 0);

    auto outcome = mvlab::load_project(file_path);

    chmod(file_path.c_str(), 0644);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::permission_denied);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: save_project reports permission_denied for an unwritable directory", "[project_model]")
{
    if (getuid() == 0) {
        return; // root bypasses POSIX permission checks; skip under CI running as root.
    }

    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "unwritable";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);
    chmod(temp_dir.c_str(), 0500); // read + execute, no write

    auto project = mvlab::create_project("Unwritable").value();
    auto outcome = mvlab::save_project(project, (temp_dir / "project.json").string());

    chmod(temp_dir.c_str(), 0700);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::permission_denied);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: save_project preserves filesystem diagnostics in details when directory creation fails", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "blocked_by_file";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir.parent_path());

    // Create a regular file at the path a directory needs to occupy, so
    // create_directories() cannot proceed.
    {
        std::ofstream blocker(temp_dir);
        blocker << "not a directory";
    }

    auto project = mvlab::create_project("Blocked").value();
    auto outcome = mvlab::save_project(project, (temp_dir / "project.json").string());

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::filesystem_error);
    REQUIRE(outcome.error().details.has_value());
    CHECK_FALSE(outcome.error().details.value().empty());

    fs::remove(temp_dir);
}

TEST_CASE("Project model: create_project has empty assets", "[project_model]")
{
    auto outcome = mvlab::create_project("Assets Test");

    REQUIRE(outcome.has_value());
    const auto& project = outcome.value();
    REQUIRE(project.assets.empty());
}

TEST_CASE("Project model: save_project writes empty assets array", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "empty_assets";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Empty Assets Test").value();
    auto save_outcome = mvlab::save_project(project, file_path);

    REQUIRE(save_outcome.has_value());

    std::ifstream file(file_path);
    nlohmann::json j;
    file >> j;
    file.close();

    REQUIRE(j.contains("assets"));
    REQUIRE(j["assets"].is_array());
    REQUIRE(j["assets"].empty());

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: project with one asset round trips", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "one_asset";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("One Asset Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-001",
        mvlab::MediaAssetType::audio,
        "Background Music",
        "media/audio/bg.mp3",
        5000000
    });

    auto save_outcome = mvlab::save_project(project, file_path);
    REQUIRE(save_outcome.has_value());

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    const auto& loaded = load_outcome.value();

    REQUIRE(loaded.assets.size() == 1);
    CHECK(loaded.assets[0].id == "asset-001");
    CHECK(loaded.assets[0].type == mvlab::MediaAssetType::audio);
    CHECK(loaded.assets[0].display_name == "Background Music");
    CHECK(loaded.assets[0].relative_path == "media/audio/bg.mp3");
    CHECK(loaded.assets[0].file_size == 5000000);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: project with multiple assets preserves order", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "multi_assets";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Multi Asset Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-001", mvlab::MediaAssetType::audio, "Music", "media/audio.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-002", mvlab::MediaAssetType::image, "Cover", "media/cover.png", 256000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-003", mvlab::MediaAssetType::video, "Background", "media/video.mp4", 50000000
    });

    auto save_outcome = mvlab::save_project(project, file_path);
    REQUIRE(save_outcome.has_value());

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    const auto& loaded = load_outcome.value();

    REQUIRE(loaded.assets.size() == 3);
    CHECK(loaded.assets[0].id == "asset-001");
    CHECK(loaded.assets[1].id == "asset-002");
    CHECK(loaded.assets[2].id == "asset-003");

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: load_project handles missing assets field (backward compatibility)", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "no_assets_field";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 1;
    j["name"] = "Old Project";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 1920}, {"height", 1080}, {"fps", 30}};

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    const auto& loaded = load_outcome.value();
    CHECK(loaded.assets.empty());

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: malformed assets field type fails", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "bad_assets_type";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 1;
    j["name"] = "Bad Assets";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 1920}, {"height", 1080}, {"fps", 30}};
    j["assets"] = "not an array";

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE_FALSE(load_outcome.has_value());
    CHECK(load_outcome.error().code == mvlab::ErrorCode::malformed_project);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: malformed asset entry fails", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "bad_asset_entry";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 1;
    j["name"] = "Bad Asset Entry";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 1920}, {"height", 1080}, {"fps", 30}};
    j["assets"] = nlohmann::json::array();
    j["assets"].push_back(nlohmann::json{
        {"id", "asset-001"},
        {"type", "audio"},
        {"display_name", "Music"}
    });

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE_FALSE(load_outcome.has_value());
    CHECK(load_outcome.error().code == mvlab::ErrorCode::malformed_project);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: unknown media type in asset fails", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "bad_type";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 1;
    j["name"] = "Bad Type";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 1920}, {"height", 1080}, {"fps", 30}};
    j["assets"] = nlohmann::json::array();
    j["assets"].push_back(nlohmann::json{
        {"id", "asset-001"},
        {"type", "unknown"},
        {"display_name", "Bad Asset"},
        {"relative_path", "media/file"},
        {"file_size", 1000}
    });

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE_FALSE(load_outcome.has_value());
    CHECK(load_outcome.error().code == mvlab::ErrorCode::malformed_project);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: invalid asset validation propagates through project validation", "[project_model]")
{
    auto project = mvlab::create_project("Invalid Asset").value();
    project.assets.push_back(mvlab::MediaAsset{
        "",
        mvlab::MediaAssetType::audio,
        "Music",
        "media/audio.mp3",
        1000000
    });

    auto outcome = mvlab::validate_project(project);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(outcome.error().message.find("ID cannot be empty") != std::string::npos);
}

TEST_CASE("Project model: duplicate asset IDs are rejected", "[project_model]")
{
    auto project = mvlab::create_project("Duplicate IDs").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-001", mvlab::MediaAssetType::audio, "Track 1", "media/track1.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-001", mvlab::MediaAssetType::audio, "Track 2", "media/track2.mp3", 2000000
    });

    auto outcome = mvlab::validate_project(project);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(outcome.error().message.find("Duplicate asset ID") != std::string::npos);
}

TEST_CASE("Project model: two distinct IDs with identical fields are accepted", "[project_model]")
{
    auto project = mvlab::create_project("Distinct IDs").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-001", mvlab::MediaAssetType::audio, "Music", "media/audio.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-002", mvlab::MediaAssetType::audio, "Music", "media/audio.mp3", 1000000
    });

    auto outcome = mvlab::validate_project(project);

    REQUIRE(outcome.has_value());
}

TEST_CASE("Project model: save_project with assets round trip preserves all fields", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "assets_roundtrip";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Full Asset Test").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-audio-1",
        mvlab::MediaAssetType::audio,
        "Main Soundtrack",
        "media/audio/main.wav",
        10485760
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-image-2",
        mvlab::MediaAssetType::image,
        "Thumbnail",
        "media/images/thumb.jpg",
        51200
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-video-3",
        mvlab::MediaAssetType::video,
        "Background Loop",
        "media/videos/loop.mov",
        314572800
    });

    auto save_outcome = mvlab::save_project(project, file_path);
    REQUIRE(save_outcome.has_value());

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    const auto& loaded = load_outcome.value();

    REQUIRE(loaded.assets.size() == 3);

    CHECK(loaded.assets[0].id == "asset-audio-1");
    CHECK(loaded.assets[0].type == mvlab::MediaAssetType::audio);
    CHECK(loaded.assets[0].display_name == "Main Soundtrack");
    CHECK(loaded.assets[0].relative_path == "media/audio/main.wav");
    CHECK(loaded.assets[0].file_size == 10485760);

    CHECK(loaded.assets[1].id == "asset-image-2");
    CHECK(loaded.assets[1].type == mvlab::MediaAssetType::image);
    CHECK(loaded.assets[1].display_name == "Thumbnail");
    CHECK(loaded.assets[1].relative_path == "media/images/thumb.jpg");
    CHECK(loaded.assets[1].file_size == 51200);

    CHECK(loaded.assets[2].id == "asset-video-3");
    CHECK(loaded.assets[2].type == mvlab::MediaAssetType::video);
    CHECK(loaded.assets[2].display_name == "Background Loop");
    CHECK(loaded.assets[2].relative_path == "media/videos/loop.mov");
    CHECK(loaded.assets[2].file_size == 314572800);

    fs::remove_all(temp_dir);
}

// ===== Project schema v2: timeline persistence =====

TEST_CASE("Project model: newly created project has empty timeline", "[project_model][timeline_persistence]")
{
    auto project = mvlab::create_project("Timeline Test").value();
    REQUIRE(project.timeline.tracks.empty());
}

TEST_CASE("Project model: save_project writes schema v2", "[project_model][timeline_persistence]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "schema_v2_write";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Schema V2 Test").value();
    auto save_outcome = mvlab::save_project(project, file_path);
    REQUIRE(save_outcome.has_value());

    std::ifstream file(file_path);
    nlohmann::json j;
    file >> j;
    file.close();

    REQUIRE(j.contains("schema_version"));
    CHECK(j["schema_version"] == 2);
    REQUIRE(j.contains("timeline"));
    CHECK(j["timeline"]["schema_version"] == 2);
    CHECK(j["timeline"]["tracks"].is_array());
    CHECK(j["timeline"]["tracks"].empty());

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: schema-v2 save/load round trip", "[project_model][timeline_persistence]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "schema_v2_roundtrip";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Round Trip V2").value();
    auto save_outcome = mvlab::save_project(project, file_path);
    REQUIRE(save_outcome.has_value());

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    CHECK(load_outcome.value().schema_version == 2);
    CHECK(load_outcome.value().timeline.tracks.empty());

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: populated mixed timeline survives project round trip", "[project_model][timeline_persistence]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "mixed_timeline_roundtrip";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Mixed Timeline").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Music", "media/audio.mp3", 1000000
    });
    project.assets.push_back(mvlab::MediaAsset{
        "asset-2", mvlab::MediaAssetType::video, "Clip", "media/clip.mp4", 2000000
    });

    project.timeline.tracks.push_back(mvlab::Track{
        "track-1",
        mvlab::TrackType::audio,
        "Main Audio",
        {mvlab::MediaClip{"clip-1", "asset-1", 0, 0, 5000000}},
        {}
    });
    project.timeline.tracks.push_back(mvlab::Track{
        "track-2",
        mvlab::TrackType::video,
        "Background",
        {mvlab::MediaClip{"clip-2", "asset-2", 0, 0, 3000000}},
        {}
    });
    auto default_presentation = mvlab::make_text_presentation_preset(mvlab::TextPresentationPreset::clean_centered).value();
    project.timeline.tracks.push_back(mvlab::Track{
        "track-3",
        mvlab::TrackType::text,
        "Lyrics",
        {},
        {
            mvlab::TextClip{"clip-3", "Verse one", 0, 2000000, default_presentation},
            mvlab::TextClip{"clip-4", "Verse two", 2000000, 2000000, default_presentation}
        }
    });

    auto save_outcome = mvlab::save_project(project, file_path);
    REQUIRE(save_outcome.has_value());

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    const auto& loaded = load_outcome.value();

    REQUIRE(loaded.timeline.tracks.size() == 3);
    CHECK(loaded.timeline.tracks[0].id == "track-1");
    CHECK(loaded.timeline.tracks[0].media_clips[0].asset_id == "asset-1");
    CHECK(loaded.timeline.tracks[1].id == "track-2");
    CHECK(loaded.timeline.tracks[1].media_clips[0].asset_id == "asset-2");
    REQUIRE(loaded.timeline.tracks[2].text_clips.size() == 2);
    CHECK(loaded.timeline.tracks[2].text_clips[0].text == "Verse one");
    CHECK(loaded.timeline.tracks[2].text_clips[1].text == "Verse two");

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: invalid timeline prevents save", "[project_model][timeline_persistence]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "invalid_timeline_save";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Invalid Timeline").value();
    project.timeline.tracks.push_back(mvlab::Track{
        "track-1",
        mvlab::TrackType::audio,
        "A",
        {
            mvlab::MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
            mvlab::MediaClip{"clip-2", "asset-2", 500000, 0, 1000000}
        },
        {}
    });

    auto save_outcome = mvlab::save_project(project, file_path);
    REQUIRE_FALSE(save_outcome.has_value());
    CHECK(save_outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK_FALSE(fs::exists(file_path));

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: malformed timeline JSON prevents load", "[project_model][timeline_persistence]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "malformed_timeline_load";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 2;
    j["name"] = "Malformed Timeline";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 1920}, {"height", 1080}, {"fps", 30}};
    j["assets"] = nlohmann::json::array();
    j["timeline"] = {
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "audio"},
                {"name", "A"}
                // missing media_clips/text_clips
            }
        })}
    };

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE_FALSE(load_outcome.has_value());
    CHECK(load_outcome.error().code == mvlab::ErrorCode::malformed_project);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: unsupported timeline schema prevents load", "[project_model][timeline_persistence]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "unsupported_timeline_schema";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 2;
    j["name"] = "Bad Timeline Schema";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 1920}, {"height", 1080}, {"fps", 30}};
    j["assets"] = nlohmann::json::array();
    j["timeline"] = {
        {"schema_version", 999},
        {"tracks", nlohmann::json::array()}
    };

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE_FALSE(load_outcome.has_value());
    CHECK(load_outcome.error().code == mvlab::ErrorCode::unsupported_schema);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: existing project data and assets remain unchanged by timeline addition", "[project_model][timeline_persistence]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "timeline_preserves_data";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Preserve Data").value();
    project.audio.path = "/path/to/audio.mp3";
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Music", "media/audio.mp3", 1000000
    });

    auto save_outcome = mvlab::save_project(project, file_path);
    REQUIRE(save_outcome.has_value());

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    const auto& loaded = load_outcome.value();
    CHECK(loaded.audio.path == "/path/to/audio.mp3");
    REQUIRE(loaded.assets.size() == 1);
    CHECK(loaded.assets[0].id == "asset-1");
    CHECK(loaded.timeline.tracks.empty());

    fs::remove_all(temp_dir);
}

// ===== Schema-v1 migration =====

TEST_CASE("Project model: valid schema-v1 project still loads", "[project_model][migration]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "v1_still_loads";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 1;
    j["name"] = "Legacy Project";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 1920}, {"height", 1080}, {"fps", 30}};
    j["assets"] = nlohmann::json::array();
    // No "timeline" field: pre-timeline schema-v1 shape.

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    CHECK(load_outcome.value().name == "Legacy Project");

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: migrated v1 project receives an empty valid timeline", "[project_model][migration]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "v1_migrated_timeline";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 1;
    j["name"] = "Legacy Project";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 1920}, {"height", 1080}, {"fps", 30}};
    j["assets"] = nlohmann::json::array();

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    CHECK(load_outcome.value().timeline.tracks.empty());
    CHECK(mvlab::validate_timeline(load_outcome.value().timeline).has_value());

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: saving a migrated v1 project writes schema v2", "[project_model][migration]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "v1_resave_writes_v2";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 1;
    j["name"] = "Legacy Project";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 1920}, {"height", 1080}, {"fps", 30}};
    j["assets"] = nlohmann::json::array();

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());

    auto resave_outcome = mvlab::save_project(load_outcome.value(), file_path);
    REQUIRE(resave_outcome.has_value());

    std::ifstream reread(file_path);
    nlohmann::json rewritten;
    reread >> rewritten;
    reread.close();

    CHECK(rewritten["schema_version"] == 2);
    REQUIRE(rewritten.contains("timeline"));
    CHECK(rewritten["timeline"]["tracks"].empty());

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: migration preserves all old project fields and assets", "[project_model][migration]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "v1_migration_preserves_fields";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 1;
    j["name"] = "Legacy With Assets";
    j["audio"] = {{"path", "/path/to/audio.mp3"}};
    j["background"] = {{"path", "/path/to/bg.mp4"}, {"type", "video"}};
    j["lyrics"] = {{"path", "/path/to/lyrics.lrc"}, {"format", "lrc"}};
    j["export_settings"] = {{"width", 1280}, {"height", 720}, {"fps", 24}};
    j["assets"] = nlohmann::json::array();
    j["assets"].push_back(nlohmann::json{
        {"id", "asset-1"},
        {"type", "audio"},
        {"display_name", "Old Asset"},
        {"relative_path", "media/old.mp3"},
        {"file_size", 12345}
    });

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE(load_outcome.has_value());
    const auto& loaded = load_outcome.value();

    CHECK(loaded.name == "Legacy With Assets");
    CHECK(loaded.audio.path == "/path/to/audio.mp3");
    CHECK(loaded.background.path == "/path/to/bg.mp4");
    CHECK(loaded.background.type == "video");
    CHECK(loaded.lyrics.path == "/path/to/lyrics.lrc");
    CHECK(loaded.lyrics.format == "lrc");
    CHECK(loaded.export_settings.width == 1280);
    CHECK(loaded.export_settings.height == 720);
    CHECK(loaded.export_settings.fps == 24);
    REQUIRE(loaded.assets.size() == 1);
    CHECK(loaded.assets[0].id == "asset-1");
    CHECK(loaded.assets[0].display_name == "Old Asset");
    CHECK(loaded.timeline.tracks.empty());

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: malformed old-schema project still fails appropriately", "[project_model][migration]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "v1_malformed_still_fails";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    nlohmann::json j;
    j["schema_version"] = 1;
    j["name"] = "Malformed Legacy";
    j["audio"] = nlohmann::json::object();
    j["background"] = nlohmann::json::object();
    j["lyrics"] = nlohmann::json::object();
    j["export_settings"] = {{"width", 0}, {"height", 1080}, {"fps", 30}};
    j["assets"] = nlohmann::json::array();

    std::ofstream file(file_path);
    file << j.dump(2);
    file.close();

    auto load_outcome = mvlab::load_project(file_path);
    REQUIRE_FALSE(load_outcome.has_value());
    CHECK(load_outcome.error().code == mvlab::ErrorCode::malformed_project);

    fs::remove_all(temp_dir);
}

// ===== Asset references from media clips =====

TEST_CASE("Project model: valid media asset reference succeeds", "[project_model][asset_references]")
{
    auto project = mvlab::create_project("Valid Reference").value();
    project.assets.push_back(mvlab::MediaAsset{
        "asset-1", mvlab::MediaAssetType::audio, "Music", "media/audio.mp3", 1000000
    });
    project.timeline.tracks.push_back(mvlab::Track{
        "track-1",
        mvlab::TrackType::audio,
        "A",
        {mvlab::MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
        {}
    });

    auto outcome = mvlab::validate_project(project);
    REQUIRE(outcome.has_value());
}

TEST_CASE("Project model: missing media asset reference fails", "[project_model][asset_references]")
{
    auto project = mvlab::create_project("Missing Reference").value();
    project.timeline.tracks.push_back(mvlab::Track{
        "track-1",
        mvlab::TrackType::audio,
        "A",
        {mvlab::MediaClip{"clip-1", "asset-does-not-exist", 0, 0, 1000000}},
        {}
    });

    auto outcome = mvlab::validate_project(project);
    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::invalid_argument);
    CHECK(outcome.error().message.find("unknown asset") != std::string::npos);
}

TEST_CASE("Project model: text-only timeline succeeds without assets", "[project_model][asset_references]")
{
    auto project = mvlab::create_project("Text Only").value();
    auto default_presentation = mvlab::make_text_presentation_preset(mvlab::TextPresentationPreset::clean_centered).value();
    project.timeline.tracks.push_back(mvlab::Track{
        "track-1",
        mvlab::TrackType::text,
        "Lyrics",
        {},
        {mvlab::TextClip{"clip-1", "Hello", 0, 1000000, default_presentation}}
    });

    auto outcome = mvlab::validate_project(project);
    REQUIRE(outcome.has_value());
}
