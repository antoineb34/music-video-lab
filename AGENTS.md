# MusicVideoLab Agent Instructions

## Project identity

- Product name: MusicVideoLab
- Repository name: music-video-lab
- Executable name: mvlab
- Language: C++23
- Build system: CMake
- Primary platform: Linux (POSIX)

## Current project stage

The project is intentionally minimal with focused command implementation.

Current scope:

- One executable (`mvlab`)
- CLI11-based command dispatcher
- One primary command: `audio inspect` for ffprobe-based audio inspection
- Unit and integration tests using Catch2
- GitHub Actions CI workflow

## Source and test file responsibilities

### Production source code

- `src/main.cpp` — CLI11 application setup and command dispatch; no process management or parsing logic
- `src/audio_inspector.hpp` — Public interface for audio inspection (minimal header)
- `src/audio_inspector.cpp` — Audio file inspection implementation using ffprobe subprocess, POSIX process primitives, and output parsing

### Test files

- `tests/smoke_tests.cpp` — CLI integration tests (no dependency on real audio files)
- `tests/audio_inspector_tests.cpp` — Unit tests for `mvlab::inspect_audio()` with temporary generated test audio files

## External tool requirements

- **ffmpeg** — Generate test audio files (required for tests)
- **ffprobe** — Inspect audio file properties (required for `audio inspect` command)

Both are invoked as external subprocesses; no FFmpeg libraries are linked.

## Linux/POSIX status

Audio inspection is Linux/Fedora-specific and uses POSIX primitives:
- `pipe()` for subprocess I/O capture
- `fork()` and `execvp()` for subprocess spawning
- `dup2()` for stream redirection
- `waitpid()` for process synchronization

This is not portable to Windows. Refactoring to use platform-neutral process APIs is future work.

## Build and test commands

**Configure:**
```
cmake -S . -B build
```

**Build:**
```
cmake --build build
```

**Run all tests:**
```
ctest --test-dir build --output-on-failure
```

**Run with Ninja (CI default):**
```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

## Testing rules

- **Temporary test media:** Generated on-the-fly with ffmpeg in `/tmp`; auto-cleaned up after each test
- **Personal file paths:** Never hardcoded in tests; manual verification uses files from `~/Music` but test code remains generic
- **CLI tests:** Use subprocess invocation; verify help, version, and argument validation
- **Unit tests:** Direct function calls; verify audio inspection logic with generated samples
- **No fixtures:** Tests generate all required temporary files dynamically

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
