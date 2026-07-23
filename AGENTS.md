# MusicVideoLab Agent Instructions

## Project identity

- Product name: MusicVideoLab
- Repository name: music-video-lab
- Executable name: mvlab
- Language: C++23
- Build system: CMake
- Primary platform: Linux (POSIX)

## Current project stage

The project has completed foundational audio inspection and project persistence subsystems.

Current scope:

- One executable (`mvlab`)
- CLI11-based command dispatcher
- Audio subsystem: `audio inspect` (ffprobe-based), `audio analyze` (PCM analysis)
- Project subsystem: `project create`, `project info` with JSON persistence
- Unit and integration tests using Catch2
- GitHub Actions CI workflow

Planned subsystems:

- Lyrics transcription and alignment
- Background media import and processing
- Timeline and preview rendering
- Video export pipeline

## Source and test file responsibilities

### Production source code

- `src/main.cpp` — CLI11 application setup and command dispatch; no process management, parsing, or project logic
- `src/audio_inspector.hpp` — Public interface for audio inspection (minimal header)
- `src/audio_inspector.cpp` — Audio file inspection implementation using ffprobe subprocess, POSIX process primitives, and output parsing
- `src/audio_analyzer.hpp` — Public interface for audio PCM analysis
- `src/audio_analyzer.cpp` — Audio PCM analysis implementation using ffmpeg subprocess for decoding
- `src/project_model.hpp` — Data structures for project persistence (Project, Audio, Background, Lyrics, ExportSettings)
- `src/project_model.cpp` — Project CRUD operations and validation logic

### Test files

- `tests/smoke_tests.cpp` — CLI integration tests (no dependency on real audio files)
- `tests/audio_inspector_tests.cpp` — Unit tests for `mvlab::inspect_audio()` with temporary generated test audio files
- `tests/audio_analyzer_tests.cpp` — Unit tests for `mvlab::analyze_audio()` with temporary generated test audio files
- `tests/project_model_tests.cpp` — Unit tests for project model serialization, validation, and CRUD operations

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

## Git workflow

Development is organized around complete features rather than individual tasks. A feature branch remains active across multiple related prompts and tasks until the feature is complete and ready to merge.

**Branch strategy:**
- `main` is protected and should remain stable; never develop directly on it.
- One feature branch per coherent feature, subsystem, or milestone: `feature/project-model`, `feature/lyrics-transcription`, `feature/background-media`, etc.
- Use small, focused commits as work progresses on the branch.
- Multiple related tasks and prompts remain on the same branch until the feature is complete.

**Commit discipline during development:**
- Make focused, atomic commits with clear messages.
- Commits need not wait for feature completion; create a new local commit when a logical chunk of work is done.
- Do not push after every prompt. The branch remains local until the feature is ready to publish.

**Publication criteria:**
A feature is ready to publish when:
- Its intended user-facing or architectural scope is complete.
- Normal success paths are implemented.
- Important failure paths are handled.
- Relevant automated tests pass and existing tests still pass.
- Compiler warnings are clean.
- Documentation is updated where needed.
- No known partial implementation is being presented as complete.
- The working tree is clean.

**Before publication, inspect:**
```bash
git status
git log --oneline main..HEAD
git diff --stat main...HEAD
git diff main...HEAD
```

**Publication workflow (when feature is complete):**
```
git push -u origin <feature-branch>
gh pr create --fill
gh pr checks --watch
gh pr merge --squash --delete-branch
git switch main
git pull
```

**During feature development (no publication):**
- Implement requested work.
- Run relevant tests.
- Create focused local commits.
- Clean up working tree.
- Report branch name, commits ahead, and work summary.
- Do not push, open a PR, merge, or delete the branch.

**Absolute rules:**
- Never force-push or delete `main`.
- Do not bypass branch protection.
- Do not publish a feature that is incomplete, untested, or documented as partial.
- Publish only when explicitly accepted or when user requests it.

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
- Create focused commits as work progresses; do not push until feature is complete.
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
