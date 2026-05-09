#documentation #contributors #ccm3

# Documentation Index

This folder contains contributor documentation for Card Collection Manager 3. Start with [Intro for New Developers](intro-to-new-developers.md) if you are new to the repository, then use the category sections below to navigate to specific tasks.

## CI/CD And Release

- [ci-cd-guide.md](ci-cd-guide.md): CI workflows, merge guards, artifacts, and release execution.
- [versioning.md](versioning.md): feature-build version format, semantic release rules on `master`, and tag/app-version behavior.
- [windows-installer.md](windows-installer.md): how the NSIS-based Windows installer is built, configured, and versioned.

## Build And Local Setup

- [dow-doc-build-locally.md](dow-doc-build-locally.md): local build setup for Windows/Linux, dependency model, build options, and troubleshooting.

## Onboarding & Development Workflows

- [intro-to-new-developers.md](intro-to-new-developers.md): architecture orientation, boundaries, common pitfalls, and first-week workflow guidance.

- [testing-and-test-code-of-conduct.md](testing-and-test-code-of-conduct.md): test workflow plus repository rules for deterministic, behavior-focused tests.

- [adding-a-new-game.md](adding-a-new-game.md): canonical end-to-end procedure for adding a new game module across `core/`, `ui_wx/`, and `app/`.

- [assets-and-info-apis.md](assets-and-info-apis.md): external info and asset APIs used by Magic, Pokemon, and Yu-Gi-Oh! modules and their runtime purpose.

## Performance & Caching

- [caching.md](caching.md): preview image caching (in-memory LRU, on-disk byte cache, HTTP connection reuse), lookup order, keys, eviction, and what is intentionally not cached.

