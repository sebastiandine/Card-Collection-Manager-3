# AGENTS.md

Long-form contributor documentation that lives outside the source tree.

## Files

- `adding-a-new-game.md` — the **canonical, end-to-end procedure** for adding a brand-new game module to the project. Covers required external resources (sets API, optional card-preview API, icons), domain type with stable JSON, set source + tests, optional card preview source + tests, `IGameModule` glue, `Game` enum + `dirNameForGame` wiring, deriving the three UI panel templates (`BaseCardListPanel<TCard, TSortColumn>`, `BaseCardEditDialog<TCard>`, `BaseSelectedCardPanel<TCard>`), the `IGameView` adapter, composition-root wiring in `app/main.cpp`, CMake additions, AGENTS.md updates, and the test checklist.
- `ci-cd-guide.md` — user-facing CI/CD and release procedure: feature/master workflow split, orchestrator + reusable platform workflow structure, versioning conventions, PR title guard for `master`, artifact naming, and a concrete "create a new release" checklist.
- `versioning.md` — dedicated versioning reference: branch+sha scheme for feature builds, semantic version bump rules for `master`, tag format, and embedded app version behavior.
- `windows-installer.md` — how the NSIS-based Windows installer (`scripts/installer.nsi`) is built and configured: the `APP_VERSION` define, where the version is surfaced (window title, branding, Programs and Features), installed sections, registry layout, and CI vs. local invocation.
- `dow-doc-build-locally.md` — complete local build/setup reference for Windows and Linux, including dependency management and troubleshooting.
- `intro-to-new-developers.md` — onboarding map for new contributors: architecture, folder responsibilities, guardrails, anti-patterns, and links to deeper docs.
- `testing-and-test-code-of-conduct.md` — testing workflow plus expected standards for writing and maintaining deterministic, hermetic, behavior-focused tests.
- `assets-and-info-apis.md` — reference for the external info APIs (set metadata) and asset APIs (card preview images) used by the Magic and Pokemon modules, plus the runtime flow through `SetService` / `CardPreviewService` and the error-surface conventions.
- `README.md` — index page that clusters docs by area and links to all documents in this directory.

## Subdirectories

- `assets/images/` — static screenshots and other binary assets referenced from the documentation (currently `demo-mtg.png`, `demo-pkm.png`). Keep filenames stable so cross-doc links don't break, and prefer compressed PNG/JPEG over uncompressed formats.

## Conventions

- These docs are reference material for contributors, not user-facing release notes. Keep them precise and dated implicitly by the source state they describe.
- Code examples should be C++20 and quote real file paths from the repo.
- Do **not** introduce dependencies on a specific upcoming feature, hypothetical game, or unmerged branch. The procedure must always describe the current code as it is on `main`.

## Required follow-ups

- After changing per-game seams in `core/` (e.g. `IGameModule`, `ISetSource`, `ICardPreviewSource`, `CollectionService`, `SetService`, `CardPreviewService`, `ImageService`) you **must** update `adding-a-new-game.md` to keep the canonical procedure in sync. The same applies to the UI seams (`IGameView`, `BaseCardListPanel`, `BaseCardEditDialog`, `BaseSelectedCardPanel`) and the composition-root wiring in `app/main.cpp`.
- After changing the Magic or Pokemon set/preview adapters (`MagicSetSource`, `MagicCardPreviewSource`, `PokemonSetSource`, `PokemonCardPreviewSource`) — endpoints, response parsing, name/number normalization, or the info-vs-asset split — you **must** update `assets-and-info-apis.md` so the API reference matches the live behavior.
- After bumping a key dependency (`nlohmann/json`, `cpr`, `wxWidgets`, `doctest`) in a way that changes a public API used in the guide's examples, update those examples.
- After adding a new file under `docs/` (or a new entry under `docs/assets/images/`) you **must** add it to the file list above **and** to `README.md` so the index stays complete.
- Do **not** rename, move, or split this file without first updating every other `AGENTS.md` that points at it (root, `core/`, `ui_wx/`, `app/`, `tests/`).

## Anti-patterns

- Don't sneak hypothetical or in-progress games into the doc as concrete examples; the guide is meant to be game-agnostic and must read as such.
- Don't link to the original Rust/Tauri repo as the source of truth — it is a historical reference, not the spec. The C++ code under `core/`, `ui_wx/`, and `app/` is the spec.
- Don't duplicate the per-package `AGENTS.md` content here; cross-link instead.
