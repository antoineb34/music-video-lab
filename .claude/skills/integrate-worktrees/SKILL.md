---
name: integrate-worktrees
description: Integrate one or two completed feature branches (typically each developed in its own git worktree) into main via cherry-pick, verify with a full clean build and test cycle, and optionally publish through a PR with a squash merge. Handles shared-file conflicts (e.g. CMakeLists.txt) by unioning both sides, checks for automated attribution before touching commit messages, and explains why squash merges make simple ancestry/patch-id checks unreliable for later verification. Use when the user wants to combine finished feature-branch work into an integration branch or into main — "integrate the two feature branches", "merge the worktree work into main", "publish the completed branches".
---

# Integrate Worktrees

Combines one or two completed, review-ready feature branches into `main` through an
integration branch, verifies the combined result with a real build and test cycle, and
optionally publishes it. This codifies the workflow this project already follows in
`AGENTS.md`'s Git workflow section — read that file before running this skill and defer
to it wherever the two disagree.

This skill supports **one branch or two branches**. Do not assume two are always present.

## Stop conditions (read these first)

Stop and surface the problem instead of proceeding if any of the following hold:

- The worktree you're running from is not on `main`, or its working tree is not clean.
- Local `main` does not match `origin/main` (after fetching).
- A named feature branch does not exist, or its expected commits (if the user gave you
  any) are not present on it.
- A feature branch's diff against `main` contains files or changes unrelated to its
  stated purpose.
- A merge conflict cannot be resolved by taking the union of both sides (i.e. resolving
  it would require dropping something either branch added).
- Required CI checks fail. Do not merge past a failing required check.
- Any step would require `--force`, force-push, rewriting a feature branch, or pushing
  directly to `main`.

None of these are "fix it and continue" situations — stop, explain what you found, and
get direction before proceeding.

## Inputs to resolve

Before starting, confirm with the user (or infer from unambiguous context and state your
assumptions back to them):

1. **Main worktree path** — where `main` is checked out.
2. **Feature branch name(s)** — one or two.
3. **Expected commits or task summary per branch** — if the user has this, use it to
   sanity-check what you find on each branch. If not, derive expectations from the
   branch's own diff and commit log.
4. **Build and test commands** — read `AGENTS.md` (or equivalent project docs) for the
   canonical configure/build/test commands; do not guess or invent them.
5. **Integration branch name** — a descriptive `integration/<topic>` name; confirm with
   the user if not given.
6. **PR title** (if publication is in scope).
7. **Whether publication is requested** — integration and verification can be a
   standalone deliverable. Do not push, open a PR, or merge unless the user has asked
   for it, explicitly or as part of this task's scope.

## Phase 1: Preflight (read-only)

All of this phase is inspection — no writes.

```bash
git fetch origin --prune
git branch --show-current
git status
git rev-parse main
git rev-parse origin/main
git worktree list

git rev-parse <feature-branch-1>
git rev-parse <feature-branch-2>   # if a second branch is in scope

git log --oneline main..<feature-branch-1>
git log --oneline main..<feature-branch-2>
```

Confirm against the stop conditions above. Then inspect each branch's diff for
unrelated changes:

```bash
git diff --stat main...<feature-branch>
git diff main...<feature-branch>
```

Stop if a branch contains anything outside its stated scope.

## Phase 2: Create the integration branch

```bash
git switch -c integration/<topic> main
```

Do not push yet. This branch is a workspace — everything from here until publication is
reversible and local.

## Phase 3: Cherry-pick feature commits

Cherry-pick each branch's commits **in the order they were made**, one branch at a time:

```bash
git cherry-pick <commit-1>
git cherry-pick <commit-2>
```

Prefer picking a branch's real, focused commits over squashing it into one — this
preserves the intent and review trail of the original work. Never edit, rebase, or
rewrite the original feature branches themselves; all replay happens on the integration
branch only.

## Phase 4: Conflict resolution

A conflict in a shared file (most commonly a build file like `CMakeLists.txt`, where
both branches independently added a new source/test entry) is expected when two
branches touch the same file. When it happens:

- **Take the union of both sides.** Add both branches' entries; keep every pre-existing
  entry from `main`. Never resolve with "ours" or "theirs" alone — that silently drops
  one side's work.
- Do not duplicate entries and do not remove existing targets or options.
- Read the fully resolved file afterward and check it by eye before continuing — a
  clean textual merge can still be logically wrong (e.g. an entry in the wrong list).

```bash
git add <resolved-file>
git cherry-pick --continue
```

If a conflict resolution would require deciding between two genuinely incompatible
changes (not just "both add a line"), stop and ask — do not guess at intent.

## Phase 5: Attribution check

Before doing anything else, check whether the commits you just replayed carry automated
attribution:

```bash
git log --format='%B' main..HEAD | grep -i "co-authored\|generated\|claude\|assistant\|anthropic"
```

- If nothing is found, continue.
- If attribution trailers are found (most often inherited automatically from the
  original feature-branch commits via cherry-pick, not something you added), **stop and
  ask for explicit approval** before rewriting any integration-branch commit message.
  Do not rewrite silently, and do not touch the original feature branches under any
  circumstance.
- If approved, rewrite only the integration-branch copies — e.g. by checking out a
  commit detached, amending its message, and cherry-picking the remainder on top — never
  `git rebase -i` (it requires interactive input this environment cannot provide).
- After rewriting, diff the reworded commits against the originals and confirm the code
  content is unchanged — only the message should differ:

```bash
git diff <original-commit> <reworded-commit>   # expect empty
```

## Phase 6: Verification

This is not optional and not satisfied by an incremental build.

```bash
git status
git log --oneline main..HEAD
git diff --stat main...HEAD
git diff --check main...HEAD
```

Inspect the combined diff. Confirm subsystems remain independent of each other unless
the task explicitly intended a cross-subsystem change, and confirm no unrelated files
(e.g. CLI entry points, persistence models) were touched by either branch.

Then run a **completely clean** build and the full test suite using this project's
canonical commands (see `AGENTS.md`):

```bash
rm -rf build
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Confirm:
- All pre-existing tests still pass.
- All new tests from each integrated branch pass.
- The combined total is higher than the pre-integration baseline by roughly the sum of
  each branch's new tests — report the **exact discovered total**, never an assumed one.
- Zero compiler warnings in project code (third-party dependency warnings, if any, are
  worth noting separately but are not this project's responsibility to fix).

Finish with:

```bash
git diff --check
git status
```

Working tree must be clean. If applicable, run any documented CLI smoke checks
(`--help` on the top-level and relevant subcommands) to confirm existing behavior is
unchanged.

## Phase 7: Publication — only when explicitly requested

If the user has not asked for publication, stop after Phase 6 and report. Otherwise:

```bash
git fetch origin
test "$(git rev-parse origin/main)" = "$(git rev-parse main)"   # must match before pushing

git push -u origin integration/<topic>
```

- Push **only** the integration branch. Never push directly to `main`. Never force-push.
- Open a PR targeting `main` with the exact verified test counts in its description —
  not an assumed or remembered number.
- Wait for required CI to pass before merging:

```bash
gh pr checks <pr-number> --watch
```

- Squash merge only after CI is green:

```bash
gh pr merge <pr-number> --squash --delete-branch
```

- Update local `main` with fast-forward-only sync — never a regular merge or rebase here:

```bash
git fetch origin --prune
git switch main
git pull --ff-only origin main
```

### A squash merge is not provable by ancestry alone

After a squash merge, `git merge-base --is-ancestor <branch> main` and
`git cherry main <branch>` will very often report the branch as **not** integrated, even
when it fully is — squashing rewrites history into a single new commit whose patch-id
won't match any individual original commit. Do not treat a nonzero result from either
check as evidence of a problem, and do not treat a zero result as the only valid proof
of integration either way. For verifying a squash-merged branch is fully present, compare
actual file content (`git show <branch>:<path>` against `main`) or full diffs instead —
this is the only reliable technique. (See the `split-main-into-worktrees` skill, which
relies on this same technique when retiring branches after a squash merge.)

## Phase 8: Preservation

Unless the user has explicitly asked for cleanup as part of this task:

- Leave both original feature branches and their worktrees untouched.
- Do not delete branches, remove worktrees, or start the next feature.
- Cleanup of completed worktrees/branches is a separate, deliberate step — see the
  `split-main-into-worktrees` skill.

## Final report

Report, using real discovered values (never placeholders or assumptions):

- Integration branch name.
- Cherry-picked commit mapping (original SHA → integration-branch SHA), per branch.
- Whether a shared-file conflict occurred, and exactly how it was resolved.
- Whether attribution was found, and whether/how it was resolved (with approval noted).
- Exact combined test total, and the per-subsystem new-test counts.
- Compiler warning status.
- If published: PR number/URL, CI result, squash-merge SHA, local/remote `main` SHA.
- Current branch and working-tree status.
- Confirmation that both original feature branches/worktrees are untouched and preserved.
- Confirmation that no automated attribution was left in the final history.
