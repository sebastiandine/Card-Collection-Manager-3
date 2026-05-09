# CI/CD Guide

This guide explains the CI/CD setup for Card Collection Manager 3 and how to create releases.

## Overview

The repository uses GitHub Actions with separate workflows:

- `feature-windows.yml`: build, test, and package Windows artifacts for non-`master` branches.
- `feature-linux.yml`: build, test, and package Linux artifacts for non-`master` branches.
- `master-pr-title-guard.yml`: validates pull request titles targeting `master`.
- `master-windows.yml`: semantic versioning, Windows build/test, tagging, and GitHub Release creation on merged PRs to `master`.

## Versioning Rules

For the detailed versioning policy and conventions, see `versioning.md`.

### Feature branches

On any push to a non-`master` branch:

- Version format: `<branch-name>-<short-sha>`
- Example: `feature-dark-mode-a1b2c3d`
- Computed by `scripts/compute_feature_version.sh`

This version is:

- used in uploaded artifact names
- baked into the app via `-DCCM_APP_VERSION=...`

### Master releases

On merged PRs into `master`:

- Semantic version is computed from the latest existing semver tag.
- Bump type is derived from PR title prefix:
  - `major...` -> major bump
  - `minor...` -> minor bump
  - `fix...` -> patch bump
  - `patch...` -> patch bump
  - `path...` -> patch bump (accepted alias in current setup)
- Computed by `scripts/compute_master_semver.sh`

The resulting version is:

- baked into the app (`CCM_APP_VERSION`)
- used in release artifact names
- tagged as `v<version>`

## Merge Guard for Master

PRs targeting `master` are validated by `master-pr-title-guard.yml`.

A PR title must start with one of:

- `major`
- `minor`
- `fix`
- `patch`
- `path`

If not, the check fails.

To enforce this as a hard merge gate, configure branch protection for `master` to require:

- `Require semver prefix in PR title`

## Artifacts

### Feature branch artifacts

- Windows workflow uploads:
  - `ccm3-windows-<version>.zip`
  - `ccm3-windows-installer-<version>`
- Linux workflow uploads:
  - `ccm3-linux-<version>.zip`

### Master release artifacts

`master-windows.yml` publishes Windows-only release assets:

- `ccm3-windows-<semver>.zip`
- `ccm3-windows-installer-<semver>.exe`

## How to Create a New Release

This is the standard release process:

1. Create a pull request targeting `master`.
2. Set the PR title prefix based on desired version bump:
   - `major: ...` for breaking changes
   - `minor: ...` for backward-compatible features
   - `fix: ...` (or `patch:` / `path:`) for bug fixes and small updates
3. Ensure all required checks pass (including title guard and build/test checks).
4. Merge the PR into `master`.
5. Wait for `master-windows.yml` to finish:
   - computes next semantic version
   - builds/tests Windows app
   - creates tag `vX.Y.Z`
   - creates GitHub Release and uploads artifacts
6. Verify in GitHub:
   - tag exists in the repository
   - release exists with expected version and assets

## Local Build Version Behavior

When building outside CI/CD:

- app version defaults to `${PROJECT_VERSION} (localbuild)`
- configured via `CCM_APP_VERSION` in top-level `CMakeLists.txt`

This makes local binaries easy to distinguish from CI/release binaries.

## Troubleshooting

- **`msys2: command not found` in Windows workflow**
  - Ensure MSYS2 setup step runs before any `run` step in jobs using `shell: msys2 {0}`.
- **`Permission denied` when linking `ccm3.exe`**
  - The executable is likely still running; close it and rebuild.
- **No release created after merge**
  - Confirm PR was merged into `master` and title prefix was valid.
  - Check `master-windows.yml` logs for versioning/tag/release step failures.
