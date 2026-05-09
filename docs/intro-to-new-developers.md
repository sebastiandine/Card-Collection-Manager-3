# Intro for New Developers

This document is a practical onboarding guide for developers new to Card Collection Manager 3.

## What this project is

Card Collection Manager 3 is a native desktop app written in C++20 with wxWidgets.

It is intentionally layered so core logic stays UI-agnostic:

- `core/` contains domain logic, services, and infrastructure adapters
- `ui_wx/` contains wxWidgets UI code only
- `app/` is the composition root that wires everything together

## Architecture at a glance

Dependencies flow inward only:

`app -> ui_wx -> core`

Rules that matter:

- `core/` must not depend on wxWidgets
- UI should consume services through `ccm::ui::AppContext`
- concrete adapter wiring belongs in `app/main.cpp`

## Where to find things

## `core/` (business/domain layer)

Main areas:

- `domain/`: value types and enums (`MagicCard`, `PokemonCard`, `Set`, `Configuration`, etc.)
- `ports/`: interfaces (`IHttpClient`, `IFileSystem`, repositories, game module seams)
- `services/`: use-case level logic (`CollectionService`, `SetService`, `ConfigService`, etc.)
- `infra/`: concrete adapters (`Json*Repository`, `StdFileSystem`, `CprHttpClient`, `LocalImageStore`)
- `games/`: per-game module implementations (`magic`, `pokemon`)

Care points:

- preserve JSON compatibility (field names/aliases are contract-sensitive)
- keep logic testable and free of UI dependencies

## `ui_wx/` (presentation layer)

Main areas:

- `MainFrame`: top-level window/menu/toolbar/split-view orchestration
- base templates:
  - `BaseCardListPanel`
  - `BaseCardEditDialog`
  - `BaseSelectedCardPanel`
- game-specific UI views/panels:
  - `Magic*`
  - `Pokemon*`
- theming and UI behavior:
  - `Theme.cpp`
  - `SvgIcons.cpp`
  - `IconListCtrl.cpp`

Care points:

- keep wx-specific code here only
- use themed popup helpers (not native light popups in dark mode flows)
- keep per-game typed wiring inside each `IGameView` implementation

## `app/` (composition root)

Main file:

- `app/main.cpp`

Responsibilities:

- instantiate concrete adapters/services/modules
- register game modules
- create `AppContext`
- create and show `MainFrame`

Care points:

- keep this as wiring code, not business logic
- preserve dependency construction/destruction order in `CcmApp`

## `tests/`

- unit tests focus on non-UI behavior
- fakes/in-memory adapters are used for deterministic tests

If you change domain JSON layout or filesystem naming behavior, update corresponding tests immediately.

## Common workflows

## Add a small feature

1. Identify layer (`core`, `ui_wx`, or both)
2. Make focused changes in the right layer
3. Run/build affected targets
4. Run tests if core behavior changed

## Add a new game

Use the canonical guide:

- `adding-a-new-game.md`

Do not improvise architecture for new games outside the existing seams.

## Release-related changes

For CI/CD, release flow, and versioning:

- `ci-cd-guide.md`
- `versioning.md`

## Build and run locally

Use:

- `dow-doc-build-locally.md`

This includes toolchains, dependencies, runtime DLL notes, options, and troubleshooting.

## What to avoid

- Adding wx headers/includes in `core/`
- Putting business logic in `app/main.cpp`
- Bypassing `AppContext` to use concrete adapters from UI code
- Changing domain JSON keys casually
- Using native `wxMessageBox` / `wxAboutBox` in flows that must match app theming
- Introducing unpinned dependency versions

## Practical “first day” checklist

1. Read `README.md` for project intent and data model context
2. Read this doc end-to-end
3. Build locally with `dow-doc-build-locally.md`
4. Skim `core/AGENTS.md`, `ui_wx/AGENTS.md`, and `app/AGENTS.md`
5. Run tests once to verify environment
6. Make a small change in one layer to get familiar with boundaries

## Related docs

- `README.md` (project overview)
- `dow-doc-build-locally.md` (local setup/build)
- `ci-cd-guide.md` (pipeline and release operations)
- `versioning.md` (version policy)
- `adding-a-new-game.md` (new game integration)
