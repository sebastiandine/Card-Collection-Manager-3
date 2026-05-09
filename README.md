# Card Collection Manager 3

Card Collection Manager 3 is an extensible desktop application for managing trading card game collections. It is designed as a practical way to track cards and manage per-card images for large collections, while preserving the established JSON layout from earlier CCM versions.

> Status: MVP. Magic the Gathering is fully supported end-to-end. Pokemon support is present and still being completed.

## What The App Does

Card Collection Manager 3 lets you maintain local card collections with per-game data, set synchronization, card image management, and desktop-first workflows.

## Technical Overview

This is a C++20 project with a wxWidgets UI:

- `core/`: domain logic, services, and infrastructure adapters
- `ui_wx/`: wxWidgets presentation layer
- `app/`: executable composition root

Dependency direction is strict: `app -> ui_wx -> core`.

## Migrating From CCM1 And CCM2

CCM3 reads the established collection layout, so data from both CCM1 and CCM2 can be copied into the configured CCM3 data directory.

Basic migration flow:

1. Close any running CCM app instance that points to the same data folder.
2. Copy your existing game data into the matching CCM3 game folder:
   - from CCM1: export/copy your collection JSON and images into the corresponding CCM3 game directory.
   - from CCM2: copy per-game `collection.json` and `images/` directories into the same per-game folders in CCM3.
3. Start CCM3 and confirm the data directory in `File > Settings`.

If your files are in the expected layout, collections should load without conversion.

## Documentation

Implementation details now live in `docs/`. Start here:

- [Documentation Index](docs/README.md)
- [Build Locally Guide](docs/dow-doc-build-locally.md)
- [Intro For New Developers](docs/intro-to-new-developers.md)
- [CI/CD Guide](docs/ci-cd-guide.md)
- [Versioning Guide](docs/versioning.md)
- [Testing Guide And Test Code Of Conduct](docs/testing-and-test-code-of-conduct.md)
- [Adding A New Game To Card Collection Manager](docs/adding-a-new-game.md)

## Previous Implementations

This project continues earlier versions of Card Collection Manager:

- [Card Collection Manager (CCM1)](https://github.com/sebastiandine/Card-Collection-Manager): original Java/Swing implementation.
- [Card Collection Manager 2 (CCM2)](https://github.com/sebastiandine/Card-Collection-Manager-2): Rust/TypeScript (Tauri) rewrite with multi-game support.

## License

This project is a personal implementation. See repository licenses and dependency
licenses for details on included code and assets.
