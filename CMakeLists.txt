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
FetchContent_MakeAvailable(Catch2 CLI11)

enable_testing()

add_executable(
    mvlab
    src/main.cpp
    src/audio_inspector.cpp
)

target_compile_options(mvlab PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

target_link_libraries(mvlab PRIVATE CLI11::CLI11)

add_executable(
    mvlab_tests
    tests/smoke_tests.cpp
    tests/audio_inspector_tests.cpp
    src/audio_inspector.cpp
)
target_include_directories(mvlab_tests PRIVATE src)
target_link_libraries(mvlab_tests PRIVATE Catch2::Catch2WithMain)
target_compile_options(mvlab_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
)

include(Catch)
catch_discover_tests(mvlab_tests)
