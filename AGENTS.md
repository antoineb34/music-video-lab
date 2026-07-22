# MusicVideoLab Agent Instructions

## Project identity

- Product name: MusicVideoLab
- Repository name: music-video-lab
- Executable name: mvlab
- Language: C++23
- Build system: CMake
- Primary platform: Fedora Linux

## Current project stage

The project is intentionally minimal.

Current scope:

- one executable
- one `main.cpp`
- one `CMakeLists.txt`
- a basic Hello World build

Do not introduce architecture, libraries, frameworks, tests, CI, packaging, or extra folders unless explicitly requested.

## Working rules

- Make only the requested change.
- Prefer the smallest correct implementation.
- Do not add speculative abstractions.
- Do not rename existing files, targets, or folders without approval.
- Do not add dependencies without approval.
- Do not commit changes.
- Do not run destructive commands outside this repository.
- Do not modify files outside this repository.
- Preserve existing behavior unless the task explicitly changes it.

## Verification

Before finishing a task:

1. Show every changed file.
2. Run the requested build command.
3. Run the executable or relevant tests.
4. Run `git diff --check`.
5. Report `git status --short`.
6. Report any warnings, errors, assumptions, or unverified behavior.

A task is not complete merely because the code was written.
