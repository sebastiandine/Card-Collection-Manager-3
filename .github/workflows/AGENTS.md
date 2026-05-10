# AGENTS.md

GitHub Actions workflows for CI, release automation, and policy checks.

## Scope

- This file governs edits in `.github/workflows/*.yml`.
- Keep one top-level triggered workflow per CI intent:
  - `feature-ci.yml` for non-`master` push CI
  - `master-ci.yml` for merged-PR-to-`master` release CI
- Keep OS/platform splits in reusable workflows invoked via `workflow_call`.

## Current workflow map

- `feature-ci.yml` -> orchestrator for feature branch CI.
- `feature-linux.yml` -> reusable Linux build/test/package workflow.
- `feature-windows.yml` -> reusable Windows build/test/package workflow.
- `master-pr-title-guard.yml` -> validates semantic-prefix policy for PRs targeting `master`.
- `master-ci.yml` -> orchestrator for merged PRs into `master`.
- `master-windows.yml` -> reusable Windows build/test/package workflow for master release flow.

## CI invariants (do not break)

1. Keep workflow intent stable:
   - feature workflows produce branch+sha artifacts
   - master workflows produce semver-tagged release artifacts
2. Do not duplicate top-level triggers for the same intent (avoid split runs in Actions UI).
3. Preserve artifact naming conventions unless docs are updated in the same change:
   - `ccm3-linux-<version>.zip`
   - `ccm3-windows-<version>.zip`
   - `ccm3-windows-installer-<version>` (feature) and `.exe` (master release asset)
4. Preserve embedded app version wiring via `-DCCM_APP_VERSION=...` in both feature and master build flows.
5. Keep `master` release flow gated to merged PRs only.
6. Keep PR title prefix policy aligned with `scripts/compute_master_semver.sh`.

## Editing rules

- Prefer minimal, surgical edits; avoid large workflow rewrites unless requested.
- Reusable workflows should declare explicit `workflow_call` inputs for required context (e.g., version, merge SHA).
- Sonar coverage steps that use `gcovr` must exclude third-party build trees at discovery time with `--exclude-directories` (for example `build/_deps`) so gcov does not process dependency `.gcda` files.
- The Sonar scan step passes `-Dsonar.coverage.exclusions=**/ui_wx/**,**/app/**` so the coverage quality gate reflects **`ccm_core_tests`** only (wx UI and the composition root are not executed under test). `sonar.sources` stays `core,ui_wx,app`.
- The Sonar scan also sets `-Dsonar.cpd.exclusions=**/ui_wx/src/*GameView.cpp,**/ui_wx/src/*CardEditDialog.cpp,**/ui_wx/src/*SelectedCardPanel.cpp` so intentionally parallel wx per-game UI scaffolding does not dominate the duplication quality gate.
- For Linux Sonar coverage jobs, keep compiler and gcov toolchain aligned; because `cmake/Toolchain.cmake` prefers Clang by default, set `-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++` explicitly in the coverage configure step when using gcovr default `gcov`.
- Keep `permissions` least-privilege:
  - reusable build workflows: `contents: read`
  - release/tag orchestrator: `contents: write`
- Keep shells consistent with runner setup:
  - Linux steps use standard bash-compatible shell.
  - Windows MSYS2 jobs keep `shell: msys2 {0}` and run `msys2/setup-msys2` before MSYS2 commands.

## Required follow-ups

- After changing workflow topology, update `docs/ci-cd-guide.md`.
- If artifact names or release behavior change, update `docs/ci-cd-guide.md` and `docs/versioning.md` if needed.
- If semver prefix policy changes, update both `master-pr-title-guard.yml` and docs.
- If adding/removing workflow files, update this file's "Current workflow map".

## Anti-patterns

- Do not add a second top-level feature or master trigger file.
- Do not move release creation into a reusable workflow that lacks `contents: write`.
- Do not introduce MSVC-specific build commands; project CI targets Clang/MinGW toolchains.
- Do not silently change artifact names; downstream release/download steps depend on exact names.
