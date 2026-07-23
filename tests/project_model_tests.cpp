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
    REQUIRE(project.schema_version == 1);
    REQUIRE(project.name == "Test Project");
    REQUIRE(!project.audio.path.has_value());
    REQUIRE(!project.background.path.has_value());
    REQUIRE(!project.background.type.has_value());
    REQUIRE(!project.lyrics.path.has_value());
    REQUIRE(!project.lyrics.format.has_value());
    REQUIRE(project.export_settings.width == 1920);
    REQUIRE(project.export_settings.height == 1080);
    REQUIRE(project.export_settings.fps == 30);
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
    project.schema_version = 2;

    auto outcome = mvlab::validate_project(project);

    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().code == mvlab::ErrorCode::unsupported_schema);
    CHECK(outcome.error().message.find("Unsupported schema version") != std::string::npos);
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
