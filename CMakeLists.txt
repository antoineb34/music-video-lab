cmake_minimum_required(VERSION 3.20)

project(
    MusicVideoLab
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.15.0
)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.6.2
)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(Catch2 CLI11 nlohmann_json)

enable_testing()

find_package(Threads REQUIRED)

# Core library containing production logic (reusable across frontends)
add_library(mvlab_core STATIC
    src/error.cpp
    src/logger.cpp
    src/audio_inspector.cpp
    src/audio_analyzer.cpp
    src/project_model.cpp
    src/media_asset.cpp
    src/timeline.cpp
    src/timeline_serialization.cpp
    src/lyrics.cpp
    src/text_presentation.cpp
    src/preview_plan.cpp
    src/preview_session.cpp
    src/media_probe.cpp
)

target_include_directories(mvlab_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mvlab_core PUBLIC
    nlohmann_json::nlohmann_json
    Threads::Threads
)

target_compile_options(mvlab_core PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

# CLI executable (depends on core library)
add_executable(
    mvlab
    src/main.cpp
)

target_compile_options(mvlab PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

target_link_libraries(mvlab PRIVATE
    mvlab_core
    CLI11::CLI11
)

# Test executable (links against core library)
add_executable(
    mvlab_tests
    tests/smoke_tests.cpp
    tests/error_tests.cpp
    tests/result_tests.cpp
    tests/logger_tests.cpp
    tests/audio_inspector_tests.cpp
    tests/audio_analyzer_tests.cpp
    tests/project_model_tests.cpp
    tests/media_asset_tests.cpp
    tests/timeline_tests.cpp
    tests/timeline_serialization_tests.cpp
    tests/lyrics_tests.cpp
    tests/text_presentation_tests.cpp
    tests/preview_plan_tests.cpp
    tests/preview_session_tests.cpp
    tests/media_probe_tests.cpp
)

target_link_libraries(mvlab_tests PRIVATE
    mvlab_core
    Catch2::Catch2WithMain
    Threads::Threads
)

target_compile_options(mvlab_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

include(Catch)
catch_discover_tests(mvlab_tests)
