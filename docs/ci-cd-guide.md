#documentation #ci-cd #github-actions

# CI/CD Guide

This guide defines the CI/CD behavior for Card Collection Manager 3. For the complete versioning policy, including prefix semantics and tag rules, see [Versioning Guide](versioning.md). For how the Windows installer artifact is produced and configured, see [Windows Installer Guide](windows-installer.md).

**Quick Setup:** protect `master` with the required check `Require semver prefix in PR title`, then keep release PR titles aligned with the accepted prefixes.

## Workflow Map

The repository uses GitHub Actions workflows split by branch intent, with one orchestrator per branch intent:

- `feature-ci.yml`: single workflow run for non-`master` pushes; orchestrates Linux and Windows feature builds.
- `feature-linux.yml`: reusable Linux build/test/package workflow invoked by `feature-ci.yml`.
- `feature-windows.yml`: reusable Windows build/test/package workflow invoked by `feature-ci.yml`.
- `master-pr-title-guard.yml`: validate PR title prefixes for PRs targeting `master`.
- `master-ci.yml`: single workflow run on merged PRs to `master`; computes semver, invokes Windows reusable build, then tags/publishes release assets.
- `master-windows.yml`: reusable Windows build/test/package workflow invoked by `master-ci.yml`.

## Version Flow

Feature branches and `master` use different version modes because they solve different problems: feature builds need traceability to a commit, while `master` builds need stable semantic releases.

### Feature Branch Builds

On pushes to non-`master` branches, `feature-ci.yml` fans out to Linux and Windows reusable workflows. Each platform workflow computes a version string in the format `<branch-name>-<short-sha>` via `scripts/compute_feature_version.sh` (for example `feature-dark-mode-a1b2c3d`).

That version is used in two places:

- artifact file names
- app embedded version via `-DCCM_APP_VERSION=...`

### Master Releases

When a PR is merged into `master`, `master-ci.yml` computes the next semantic version from the latest semver tag and the PR title prefix using `scripts/compute_master_semver.sh`.

Accepted prefix mapping:

- `major...` -> major bump
- `minor...` -> minor bump
- `fix...` -> patch bump
- `patch...` -> patch bump
- `path...` -> patch bump (accepted alias in current setup)

The resulting version is embedded in the app (`CCM_APP_VERSION`), used in artifact names, and tagged as `v<version>`.

## Master Merge Guard

PRs targeting `master` must start with one of the accepted prefixes:

- `major`
- `minor`
- `fix`
- `patch`
- `path`

If a title does not match, `master-pr-title-guard.yml` fails and the PR should not be merged.

## Artifact Naming

Feature workflow artifacts (produced by jobs inside `feature-ci.yml`):

- Windows: `ccm3-windows-<version>.zip`, `ccm3-windows-installer-<version>`
- Linux: `ccm3-linux-<version>.zip`

Master release artifacts (`master-ci.yml`, built via `master-windows.yml`):

- `ccm3-windows-<semver>.zip`
- `ccm3-windows-installer-<semver>.exe`

## Release Procedure

Follow this flow for every release:

1. Open a PR to `master`.
2. Use a valid semver prefix in the PR title (`major:`, `minor:`, `fix:`, `patch:`, or `path:`).
3. Wait for all required checks to pass.
4. Merge the PR.
5. Wait for `master-ci.yml` to complete semver computation, Windows build/test, tag creation, and release publishing.
6. Verify the `vX.Y.Z` tag and release assets in GitHub.

## Local Build Version Behavior

Outside CI, the app version defaults to `${PROJECT_VERSION} (localbuild)` through `CCM_APP_VERSION` in the top-level `CMakeLists.txt`. This keeps local binaries easy to distinguish from CI and release outputs.

## Troubleshooting

- **`msys2: command not found` in workflow logs:** ensure the MSYS2 setup step runs before jobs that use `shell: msys2 {0}`.
- **`Permission denied` while linking `ccm3.exe`:** the app is still running; close it and rebuild.
- **No release after merge:** verify the PR merged into `master` and used a valid prefix, then inspect `master-ci.yml` logs for version/tag/release failures.
