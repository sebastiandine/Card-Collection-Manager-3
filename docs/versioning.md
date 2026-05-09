#documentation #versioning #releases

# Versioning Guide

This guide defines how Card Collection Manager 3 assigns versions in CI and release flows. For workflow wiring and release execution details, see [CI/CD Guide](ci-cd-guide.md).

**Quick Setup:** choose a valid PR title prefix before opening a `master` PR because the prefix determines the release bump.

## Versioning Model

The project uses two versioning modes:

- feature-build versioning for non-`master` branches
- semantic versioning for merged PRs into `master`

## Feature-Build Versioning

Non-`master` pushes use `<branch-name>-<short-sha>` (for example `feature-dark-theme-a1b2c3d` or `fix-sort-order-f91d2ab`).

Rules:

- branch names are sanitized and lowercased
- commit SHA is shortened to 7 characters
- computation runs in `scripts/compute_feature_version.sh`

Usage:

- artifact names
- app embedded version (`CCM_APP_VERSION`, visible in `Help -> About`)

## Master Semantic Versioning

Merged PRs into `master` use semantic versions in `MAJOR.MINOR.PATCH` format (for example `1.4.2`).

CI computes the next version from the latest semver tag and the PR title prefix:

- `major...` -> bump `MAJOR`, reset `MINOR` and `PATCH` to `0`
- `minor...` -> bump `MINOR`, reset `PATCH` to `0`
- `fix...` -> bump `PATCH`
- `patch...` -> bump `PATCH`
- `path...` -> bump `PATCH` (accepted alias in current setup)

Computation runs in `scripts/compute_master_semver.sh`.

## Validation Rules

If the PR title prefix is not accepted, two controls fail by design:

- PR title guard check for `master` PRs
- semantic-version script validation

This enforcement keeps release bumps deterministic and reviewable.

## Tag Format

Master releases create git tags in this format:

- `v<semantic-version>`

Examples:

- `v1.0.0`
- `v2.3.7`

## Embedded App Version

The app embeds a build-time version string through CMake variable `CCM_APP_VERSION`.

CI behavior:

- feature workflows set it to `<branch>-<sha>`
- master release workflow sets it to semantic version

Local/manual behavior:

- default is `${PROJECT_VERSION} (localbuild)` unless overridden

This default makes local binaries easy to distinguish from CI and release outputs.

## PR Title Conventions

Use explicit prefixes in this shape:

- `major: <summary>`
- `minor: <summary>`
- `fix: <summary>`
- `patch: <summary>`

Example: `minor: add custom themed confirmation dialogs`.
