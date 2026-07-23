# MusicVideoLab Project Notes

**Read `AGENTS.md` before making changes.** It contains the repository conventions and guidelines:
- Project identity and current scope
- Source and test file responsibilities
- External tool requirements (ffmpeg, ffprobe)
- Canonical build and test commands
- Git workflow for feature-branch development
- Runtime error and logging policy (typed errors, `Result<T>`, `Logger`, exit codes)
- Testing rules and POSIX status
- Working rules and verification procedures

**Key workflow principle:** Development is organized around complete features, not individual changes. Use one feature branch for related work, make focused local commits as progress happens, and publish to `main` only when the feature is complete and ready. See `AGENTS.md` Git workflow section for full details.

**Key runtime principle:** Core code (`mvlab_core`) returns typed `Result<T>`/`Error` on expected failures and never prints directly; `main.cpp` owns all CLI formatting, logging-level wiring, and exit-code mapping. See `AGENTS.md`'s "Runtime error and logging policy" section for full details.

**General rules:**
- Preserve existing behavior unless the task changes it.
- Verify changes with the full build and test cycle.
- Follow the Git workflow documented in `AGENTS.md`.
