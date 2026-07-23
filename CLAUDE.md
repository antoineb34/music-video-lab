# MusicVideoLab Project Notes

**Read `AGENTS.md` before making changes.** It contains:
- Project identity and current scope
- Source and test file responsibilities
- External tool requirements (ffmpeg, ffprobe)
- Canonical build and test commands
- Git workflow for feature-branch development
- Testing rules and POSIX status
- Working rules and verification procedures

**Key workflow principle:** Development is organized around complete features, not individual prompts. Use one feature branch for related work, make focused local commits as progress happens, and publish to `main` only when the feature is complete and ready. See `AGENTS.md` Git workflow section for full details.

**General rules:**
- Preserve existing behavior unless the task changes it.
- Verify changes with the full build and test cycle.
- Follow the Git workflow documented in `AGENTS.md`.
