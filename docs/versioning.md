# Versioning Guide

This document defines how version numbers are assigned in Card Collection Manager 3.

For the pipeline flow (workflows, checks, tags, and release publishing), see `ci-cd-guide.md`.

## Versioning Model

The project uses two versioning modes:

- **Feature branch CI versioning** for non-`master` branches
- **Semantic versioning** for merged PRs into `master`

## Feature Branch Versioning

For pushes to non-`master` branches, version format is:

- `<branch-name>-<short-sha>`

Examples:

- `feature-dark-theme-a1b2c3d`
- `fix-sort-order-f91d2ab`

### Rules

- Branch name is sanitized and lowercased.
- Commit SHA is shortened to 7 characters.
- Computation is handled by `scripts/compute_feature_version.sh`.

### Usage

This feature version is used for:

- CI artifact naming
- embedded app version shown in `Help -> About` (via `CCM_APP_VERSION`)

For where this is wired in workflows, see `ci-cd-guide.md`.

## Master Semantic Versioning

For merged PRs into `master`, version format is semantic:

- `MAJOR.MINOR.PATCH` (for example `1.4.2`)

The next version is computed from the latest semver tag and PR title prefix.

### PR title prefix mapping

- `major...` -> bump `MAJOR`, reset `MINOR` and `PATCH` to `0`
- `minor...` -> bump `MINOR`, reset `PATCH` to `0`
- `fix...` -> bump `PATCH`
- `patch...` -> bump `PATCH`
- `path...` -> bump `PATCH` (accepted alias in current setup)

### Strict validation

If PR title prefix is not one of the accepted values:

- PR title guard check fails for `master` PRs
- semantic version script fails fast

Computation is handled by `scripts/compute_master_semver.sh`.

For enforcement and branch protection details, see `ci-cd-guide.md`.

## Tags

Master releases create a git tag in this format:

- `v<semantic-version>`

Examples:

- `v1.0.0`
- `v2.3.7`

Tag creation is part of the master release workflow (see `ci-cd-guide.md`).

## App Embedded Version

The app embeds a build-time version string through CMake variable `CCM_APP_VERSION`.

### CI builds

- Feature CI sets `CCM_APP_VERSION` to branch+sha version
- Master release CI sets it to semantic version

### Local/manual builds

If not overridden, default is:

- `${PROJECT_VERSION} (localbuild)`

This makes local binaries distinguishable from CI and release binaries.

## Recommended PR Title Conventions

Use explicit, readable prefixes:

- `major: <summary>`
- `minor: <summary>`
- `fix: <summary>`
- `patch: <summary>`

Example:

- `minor: add custom themed confirmation dialogs`

This keeps version intent clear for reviewers and release automation.
