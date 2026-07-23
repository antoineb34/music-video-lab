#include <catch2/catch_all.hpp>
#include "project_model.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

TEST_CASE("Project model: create_project creates default project", "[project_model]")
{
    auto project = mvlab::create_project("Test Project");

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

TEST_CASE("Project model: validate_project accepts valid project", "[project_model]")
{
    auto project = mvlab::create_project("Test Project");
    auto [is_valid, error] = mvlab::validate_project(project);

    REQUIRE(is_valid);
    REQUIRE(error.empty());
}

TEST_CASE("Project model: validate_project rejects empty name", "[project_model]")
{
    auto project = mvlab::create_project("Test");
    project.name = "";

    auto [is_valid, error] = mvlab::validate_project(project);

    REQUIRE(!is_valid);
    REQUIRE(error.find("name cannot be empty") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects invalid schema version", "[project_model]")
{
    auto project = mvlab::create_project("Test");
    project.schema_version = 2;

    auto [is_valid, error] = mvlab::validate_project(project);

    REQUIRE(!is_valid);
    REQUIRE(error.find("Unsupported schema version") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects zero width", "[project_model]")
{
    auto project = mvlab::create_project("Test");
    project.export_settings.width = 0;

    auto [is_valid, error] = mvlab::validate_project(project);

    REQUIRE(!is_valid);
    REQUIRE(error.find("width must be greater than zero") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects zero height", "[project_model]")
{
    auto project = mvlab::create_project("Test");
    project.export_settings.height = 0;

    auto [is_valid, error] = mvlab::validate_project(project);

    REQUIRE(!is_valid);
    REQUIRE(error.find("height must be greater than zero") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects zero fps", "[project_model]")
{
    auto project = mvlab::create_project("Test");
    project.export_settings.fps = 0;

    auto [is_valid, error] = mvlab::validate_project(project);

    REQUIRE(!is_valid);
    REQUIRE(error.find("fps must be greater than zero") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects invalid background type", "[project_model]")
{
    auto project = mvlab::create_project("Test");
    project.background.type = "invalid";

    auto [is_valid, error] = mvlab::validate_project(project);

    REQUIRE(!is_valid);
    REQUIRE(error.find("Invalid background type") != std::string::npos);
}

TEST_CASE("Project model: validate_project rejects invalid lyrics format", "[project_model]")
{
    auto project = mvlab::create_project("Test");
    project.lyrics.format = "invalid";

    auto [is_valid, error] = mvlab::validate_project(project);

    REQUIRE(!is_valid);
    REQUIRE(error.find("Invalid lyrics format") != std::string::npos);
}

TEST_CASE("Project model: validate_project accepts valid background types", "[project_model]")
{
    auto project = mvlab::create_project("Test");

    project.background.type = "image";
    auto [is_valid1, error1] = mvlab::validate_project(project);
    REQUIRE(is_valid1);

    project.background.type = "video";
    auto [is_valid2, error2] = mvlab::validate_project(project);
    REQUIRE(is_valid2);
}

TEST_CASE("Project model: validate_project accepts valid lyrics formats", "[project_model]")
{
    auto project = mvlab::create_project("Test");

    project.lyrics.format = "txt";
    auto [is_valid1, error1] = mvlab::validate_project(project);
    REQUIRE(is_valid1);

    project.lyrics.format = "lrc";
    auto [is_valid2, error2] = mvlab::validate_project(project);
    REQUIRE(is_valid2);
}

TEST_CASE("Project model: save_project and load_project round trip", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "round_trip";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto original = mvlab::create_project("Round Trip Test");
    original.audio.path = "/path/to/audio.mp3";
    original.background.path = "/path/to/bg.mp4";
    original.background.type = "video";
    original.export_settings.width = 1280;
    original.export_settings.height = 720;

    auto [save_success, save_error] = mvlab::save_project(original, file_path);
    REQUIRE(save_success);
    REQUIRE(save_error.empty());

    auto [loaded, load_error] = mvlab::load_project(file_path);
    REQUIRE(load_error.empty());
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
    auto [project, error] = mvlab::load_project("/nonexistent/path/project.json");

    REQUIRE(!error.empty());
    REQUIRE(error.find("not found") != std::string::npos);
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

    auto [project, error] = mvlab::load_project(file_path);

    REQUIRE(!error.empty());
    REQUIRE((error.find("JSON parsing") != std::string::npos ||
             error.find("parsing") != std::string::npos));

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

    auto [project, error] = mvlab::load_project(file_path);

    REQUIRE(!error.empty());
    REQUIRE(error.find("Unsupported schema version") != std::string::npos);

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: save_project creates parent directories", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "nested" / "path" / "to" / "project";
    fs::remove_all(temp_dir.parent_path());

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Nested Test");
    auto [success, error] = mvlab::save_project(project, file_path);

    REQUIRE(success);
    REQUIRE(error.empty());
    REQUIRE(fs::exists(file_path));

    fs::remove_all(temp_dir.parent_path());
}

TEST_CASE("Project model: path containing spaces", "[project_model]")
{
    auto temp_dir = fs::temp_directory_path() / "mvlab_test" / "path with spaces";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::string file_path = (temp_dir / "project.json").string();

    auto project = mvlab::create_project("Spaces Test");
    auto [save_success, save_error] = mvlab::save_project(project, file_path);

    REQUIRE(save_success);
    REQUIRE(fs::exists(file_path));

    auto [loaded, load_error] = mvlab::load_project(file_path);

    REQUIRE(load_error.empty());
    REQUIRE(loaded.name == "Spaces Test");

    fs::remove_all(temp_dir);
}

TEST_CASE("Project model: validate_project accepts null optional fields", "[project_model]")
{
    auto project = mvlab::create_project("Test");

    project.audio.path.reset();
    project.audio.analysis.reset();
    project.background.path.reset();
    project.background.type.reset();
    project.lyrics.path.reset();
    project.lyrics.format.reset();

    auto [is_valid, error] = mvlab::validate_project(project);

    REQUIRE(is_valid);
    REQUIRE(error.empty());
}
