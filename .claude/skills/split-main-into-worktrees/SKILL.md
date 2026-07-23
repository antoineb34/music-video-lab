---
name: split-main-into-worktrees
description: Create one or two fresh feature worktrees/branches from a synchronized main, optionally retiring old completed worktrees and branches first — including safe verification when the old branch was squash-merged, where simple ancestry or patch-id checks are inconclusive. Enforces one task = one branch = one worktree = one Claude session, and never force-deletes a branch or force-removes a worktree without explicit approval. Use when the user wants new worktrees for upcoming feature work, or wants to retire finished worktrees and replace them with fresh ones — "set up two new worktrees for the next features", "retire the old worktrees and start fresh branches".
---

# Split Main Into Worktrees

Sets up one or two fresh feature worktrees from a synchronized `main`, and — when old
worktrees are being retired first, often to reuse their paths — safely verifies and
removes the old ones before creating the new ones. This codifies the git-worktree
workflow this project uses: read `AGENTS.md`'s Git workflow section before running this
skill and defer to it wherever the two disagree.

This skill supports **one branch or two branches**, and works whether or not there is
anything to retire first. Do not assume a retirement step is always needed.

## The core principle

```
one task = one branch = one worktree = one Claude session
```

Each worktree is a separate checkout with its own working directory, so parallel
sessions never step on each other's uncommitted state. A branch without a worktree is
just a pointer; a worktree is where the actual files live.

## Stop conditions (read these first)

Stop and surface the problem instead of proceeding if any of the following hold:

- The main worktree is not on `main`, or its working tree is not clean.
- Any worktree in scope (old or reused-path) has uncommitted changes.
- Local `main` does not match `origin/main` (after fetching).
- A requested new branch name already exists.
- A requested new worktree path is already registered or occupied by something
  unexpected (inspect before reusing — don't assume it's safe to overwrite).
- An old branch being retired cannot be shown to be fully integrated into `main` (see
  Phase 3 for what "shown" means for squash-merged branches).
- Any step would require `git worktree remove --force` or `git branch -D` without the
  user having explicitly approved that specific deletion.

None of these are "fix it and continue" situations — stop, explain what you found, and
get direction before proceeding.

## Inputs to resolve

Before starting, confirm with the user (or infer from unambiguous context and state your
assumptions back to them):

1. **Main worktree path.**
2. **New branch name(s)** — one or two.
3. **Matching new worktree path(s)** — one per branch.
4. **Old branch/worktree mappings**, if paths are being reused and old worktrees need
   retiring first.
5. **Evidence the old work was integrated** — a PR number, merge commit, or "just ask me
   to verify" — whatever the user has; if nothing is given, verify from git state
   directly (Phase 3).

## Phase 1: Preflight (read-only)

```bash
git fetch origin --prune
git branch --show-current
git status
git rev-parse main
git rev-parse origin/main
git worktree list
```

Confirm `main` is clean and matches `origin/main`. List current worktree mappings and
check that the requested new branch names and paths are free (or, if paths are being
reused, that they currently belong to worktrees you've been told to retire — never a
worktree you weren't told about).

If retiring old worktrees, also check each one directly:

```bash
git -C <old-worktree-path> status
git -C <old-worktree-path> branch --show-current
```

## Phase 2: Safe retirement (only if retiring old worktrees)

Skip this phase entirely if the task is purely additive (no paths are being reused, no
old branches are being retired).

### Step 1 — verify integration

Try ancestry first:

```bash
git merge-base --is-ancestor <old-branch> main
echo "ancestor result: $?"
```

A zero exit code is a clean, sufficient proof of integration for a fast-forward or
regular merge. But **a nonzero result does not mean the branch is unintegrated** — if
the branch was squash-merged, its individual commits are not ancestors of `main` by
construction, since squashing rewrites history into one new commit.

When ancestry is inconclusive, also try patch-id comparison, understanding its limits:

```bash
git cherry main <old-branch>
```

`git cherry` marks a commit `+` (no equivalent found) whenever no single commit on
`main` has a matching patch-id. After a squash merge this is normal and expected even
for fully-integrated work — a squashed commit's diff is the union of several original
diffs, so it won't patch-id-match any one of them individually. Treat a `+` result here
as inconclusive, not as evidence something is missing.

**For a squash-merged branch, verify by content instead.** Compare the actual files the
branch touched against their current state on `main`:

```bash
git diff --stat main...<old-branch>
git diff <old-branch>:<path> main:<path>   # or: diff <(git show main:<path>) <(git show <old-branch>:<path>)
```

Files the branch itself introduced should be byte-identical to what's now on `main`.
Shared files the branch modified (e.g. a build file where two branches each added a
line) may legitimately differ on `main` if `main` now contains a superset of changes —
inspect those diffs by eye and confirm nothing was dropped, not just that something
changed.

Only proceed to removal once you can state specifically what you verified and why it
proves the old work is fully present on `main`. "The PR merged" is not sufficient on its
own — check the content.

### Step 2 — remove the worktree

```bash
git worktree remove <old-worktree-path>
git worktree prune
```

Never pass `--force`. If removal is refused, the worktree has uncommitted state — stop
and investigate; do not force past that warning.

### Step 3 — delete the branch

```bash
git branch -d <old-branch>
```

If this succeeds, the branch is confirmed integrated by Git's own ancestry logic — done.

If Git refuses (`not fully merged`), this is the expected outcome after a squash merge —
Git's ancestry check cannot see through a squash. Do not immediately escalate to `-D`.
Instead:

1. Re-confirm the content verification from Step 1 is solid.
2. Report to the user that safe deletion requires their explicit approval, because Git
   cannot recognize squash integration on its own, and explain what you verified.
3. Only after explicit approval, run:

```bash
git branch -D <old-branch>
```

Never force-delete a branch automatically, and never do this for a branch the user
hasn't specifically approved.

## Phase 3: Fresh setup

For each new branch/worktree pair, created directly from current `main`:

```bash
git worktree add -b <new-branch-name> <new-worktree-path> main
```

Do this once per branch (up to two). Do not create more than the user asked for, and do
not start implementation work in the new worktrees — they should be created empty and
clean, ready for the next task.

## Phase 4: Verification

For every worktree now in scope (the main worktree plus each new one):

```bash
git worktree list

git -C <worktree-path> branch --show-current
git -C <worktree-path> rev-parse HEAD
git -C <worktree-path> status
```

Confirm:

- Every new worktree's `HEAD` matches the current `main` SHA exactly.
- Every worktree is on its intended branch.
- Every worktree reports a clean working tree.
- No product source files, tests, or build configuration changed anywhere in this
  process — this skill only manipulates git branches and worktrees.
- No commits were made and nothing was pushed.

## Final report

Report, using real discovered values:

- Old worktree path(s) removed, if any.
- Branch deletion results per branch: whether lowercase `-d` succeeded on its own, or
  whether `-D` was used after explicit approval — and what content verification backed
  that approval.
- New branch → worktree path mappings.
- `HEAD` SHA and clean status of every worktree now in scope.
- Confirmation that no source files were changed and no commits or pushes occurred.
