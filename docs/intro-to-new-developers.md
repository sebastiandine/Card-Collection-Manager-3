#documentation #onboarding #architecture

# Intro For New Developers

This document orients new contributors to Card Collection Manager 3. For local setup and build commands, start with [Build Locally Guide](dow-doc-build-locally.md); then use this guide to understand architecture boundaries and daily workflows.

**Quick Setup:** read `AGENTS.md` files first, build once, run tests once, then make one small layer-scoped change to validate your environment.

## Toolchain Snapshot

Card Collection Manager 3 builds as a native C++ desktop binary with CMake. The project standard is C++20, with Clang as the preferred compiler on Windows and Linux, plus a verified MinGW-w64 fallback path on Windows.

- build system: CMake 3.22+ with `FetchContent`
- language/toolchain: C++20, Clang preferred, MinGW-w64 GCC fallback on Windows
- UI toolkit: wxWidgets (`ui_wx/` only)
- test framework: `doctest`

Use [Build Locally Guide](dow-doc-build-locally.md) for exact commands, generator options, runtime DLL notes, and troubleshooting details.

## Project Shape

Card Collection Manager 3 is a native desktop app written in C++20 with wxWidgets. The architecture is intentionally layered so core logic stays UI-agnostic.

- `core/`: domain logic, services, and infrastructure adapters
- `ui_wx/`: wxWidgets UI code only
- `app/`: composition root that wires adapters, services, and views

Dependency direction is strict: `app -> ui_wx -> core`.

## Architecture Rules

These rules are the baseline for all feature work:

- `core/` must never include or depend on wxWidgets.
- UI code consumes services through `ccm::ui::AppContext`.
- Concrete adapter wiring belongs in `app/main.cpp`.
- JSON keys and aliases are contract-sensitive and must stay stable.

## Repository Map

### `core/`

`core/` contains domain and non-UI behavior:

- `domain/`: value types and enums (`MagicCard`, `PokemonCard`, `Set`, `Configuration`)
- `ports/`: seam interfaces (`IHttpClient`, `IFileSystem`, repositories, game seams)
- `services/`: use-case logic (`CollectionService`, `SetService`, `ConfigService`)
- `infra/`: concrete adapters (`Json*Repository`, `StdFileSystem`, `CprHttpClient`, `LocalImageStore`)
- `games/`: per-game modules (`magic`, `pokemon`)

### `ui_wx/`

`ui_wx/` contains all presentation code:

- `MainFrame`: top-level shell and menu/split-view orchestration
- `BaseCardListPanel`, `BaseCardEditDialog`, `BaseSelectedCardPanel`: reusable UI templates
- `Magic*` and `Pokemon*` classes: game-specific view/panel implementations
- `Theme.cpp`, `SvgIcons.cpp`, `IconListCtrl.cpp`: theming and visual behavior

### `app/`

`app/main.cpp` is the composition root:

- instantiate adapters, services, and modules
- register game modules and game views
- build `AppContext`
- create and show `MainFrame`

Keep this file focused on wiring, not business logic.

### `tests/`

Tests target non-UI behavior with deterministic fakes and in-memory adapters. When a domain JSON contract or filesystem naming rule changes, update the matching tests in the same change.

## Common Workflows

### Add A Small Feature

1. Identify the correct layer (`core`, `ui_wx`, or both).
2. Make the smallest coherent change in that layer.
3. Rebuild the affected target.
4. Run tests when core behavior changes.

### Add A New Game

Use [Adding a new game to Card Collection Manager](adding-a-new-game.md). Do not bypass the existing seams or invent parallel architecture for a new game.

### Release-Oriented Changes

Use [CI/CD Guide](ci-cd-guide.md) and [Versioning Guide](versioning.md) for workflow and release policy decisions.

## Avoid These Pitfalls

- adding wx headers in `core/`
- putting business logic in `app/main.cpp`
- accessing concrete adapters directly from UI code instead of `AppContext`
- changing JSON key spellings casually
- using unpinned dependency versions

## First-Day Checklist

1. Read repository `AGENTS.md` files (`core/`, `ui_wx/`, `app/`, `tests/`).
2. Build locally with [Build Locally Guide](dow-doc-build-locally.md).
3. Run the test suite once.
4. Make one small layer-contained change.
5. Rebuild and rerun relevant tests.
