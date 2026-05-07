# AGENTS.md

Long-form contributor documentation that lives outside the source tree.

## Files

- `adding-a-new-game.md` — the **canonical, end-to-end procedure** for adding a brand-new game module to the project. Covers required external resources (sets API, optional card-preview API, icons), domain type with stable JSON, set source + tests, optional card preview source + tests, `IGameModule` glue, `Game` enum + `dirNameForGame` wiring, deriving the three UI panel templates (`BaseCardListPanel<TCard, TSortColumn>`, `BaseCardEditDialog<TCard>`, `BaseSelectedCardPanel<TCard>`), the `IGameView` adapter, composition-root wiring in `app/main.cpp`, CMake additions, AGENTS.md updates, and the test checklist.

## Conventions

- These docs are reference material for contributors, not user-facing release notes. Keep them precise and dated implicitly by the source state they describe.
- Code examples should be C++20 and quote real file paths from the repo.
- Do **not** introduce dependencies on a specific upcoming feature, hypothetical game, or unmerged branch. The procedure must always describe the current code as it is on `main`.

## Required follow-ups

- After changing per-game seams in `core/` (e.g. `IGameModule`, `ISetSource`, `ICardPreviewSource`, `CollectionService`, `SetService`, `CardPreviewService`, `ImageService`) you **must** update `adding-a-new-game.md` to keep the canonical procedure in sync. The same applies to the UI seams (`IGameView`, `BaseCardListPanel`, `BaseCardEditDialog`, `BaseSelectedCardPanel`) and the composition-root wiring in `app/main.cpp`.
- After bumping a key dependency (`nlohmann/json`, `cpr`, `wxWidgets`, `doctest`) in a way that changes a public API used in the guide's examples, update those examples.
- Do **not** rename, move, or split this file without first updating every other `AGENTS.md` that points at it (root, `core/`, `ui_wx/`, `app/`, `tests/`).

## Anti-patterns

- Don't sneak hypothetical or in-progress games into the doc as concrete examples; the guide is meant to be game-agnostic and must read as such.
- Don't link to the original Rust/Tauri repo as the source of truth — it is a historical reference, not the spec. The C++ code under `core/`, `ui_wx/`, and `app/` is the spec.
- Don't duplicate the per-package `AGENTS.md` content here; cross-link instead.
