# core/AGENTS.md

`ccm_core` static library — domain types, ports, services, infra adapters. Hard rule: **no UI dependencies, ever**. Read the root `AGENTS.md` first.

## Layer pointers

- `include/ccm/domain/` — POD value types: `Enums`, `Set`, `MagicCard`, `PokemonCard`, `Configuration`. Each has `to_json` / `from_json` defined in the matching `src/domain/*.cpp`.
- `include/ccm/ports/` — interfaces (`IHttpClient`, `IFileSystem`, `ICollectionRepository<T>`, `ISetRepository`, `IImageStore`, `ICardPreviewSource`). All seams the services depend on. Add new ports here when adding new external concerns.
- `include/ccm/services/` — high-level operations: `ConfigService`, `CollectionService<TCard>` (header-only template), `SetService`, `ImageService`, `CardPreviewService`, `CardSorter` (free functions; per-column sort comparators that mirror established table sorting behavior — UI-agnostic so they can be unit-tested directly), `CardFilter` (free functions; case-insensitive substring row matcher restricted to each game's `tableFields` valueKey list). They depend only on ports.
- `include/ccm/infra/` — concrete adapters: `CprHttpClient`, `StdFileSystem`, `JsonCollectionRepository<T>` (header-only template), `JsonSetRepository`, `LocalImageStore`.
- `include/ccm/games/` — `IGameModule` + per-game modules. `IGameModule` consolidates the per-game seams: every module owns an `ISetSource` (required) and may own an `ICardPreviewSource` (optional, default `nullptr`). `magic/` and `pokemon/` are the reference implementations — both expose a fully working set source + card preview source.
- `include/ccm/util/` — `Result.hpp` (the sum type), `FsNames.hpp` (filename munging ported from `util/fs.rs`).
- `src/` mirrors `include/ccm/` for non-template implementations.

## Conventions

1. **No throw across ports.** Return `ccm::Result<T>::ok(...)` / `Result<T>::err("msg")`. The caller propagates with `if (!r) return Result<T>::err(r.error());`.
2. **JSON serde stays byte-for-byte stable.** When the C++ field name differs from the JSON key (`signed_` vs `"signed"`, `releaseDate`, `setNo`, `firstEdition`, `dataStorage`, `defaultGame`), write hand-rolled `to_json` / `from_json` instead of `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` so the alias is explicit. Round-trip tests in `tests/domain_json_tests.cpp` enforce this — extend them whenever you touch a domain type.
3. **Filename rule for images** lives in `services/ImageService.hpp` and matches the Rust source exactly:
   - new entry  -> `"{set}+{name}+{idx}.{ext}"`
   - existing   -> `"{id}+{set}+{name}+{idx}.{ext}"`  
   `ImageService::buildTargetName` is the single source of truth. Don't duplicate the rule elsewhere.
4. **Templates stay header-only** (`CollectionService<T>`, `JsonCollectionRepository<T>`). Don't add `.cpp` files for them; explicit instantiation is not used.
5. **Path strings** that get persisted (e.g. `Configuration::dataStorage`) use `std::filesystem::path::generic_string()`, never `string()` — keeps `/` separators on Windows so JSON round-trips and tests stay portable.
6. **Compiler warnings**: every target in this package links `ccm_warnings` `PRIVATE`. Treat warnings as errors locally during dev (`-Werror` is opt-in but encouraged).
7. **No `wx/...` includes** in headers or sources here. Verify with `rg "wx/" core/` — must be empty.
8. **HTTP query strings must be percent-encoded** before they reach `IHttpClient::get`. `cpr::Url` does **not** encode the URL string we hand it. See `MagicCardPreviewSource::buildSearchUrl` for the canonical pattern (RFC 3986 unreserved-set encoder). `IHttpClient::get` accepts arbitrary bytes back — `Result<std::string>` is a binary buffer, not text, so callers can use it for image payloads directly.

## Adding a new game

The end-to-end procedure (core + UI + composition root + docs) lives in `docs/adding-a-new-game.md`. The core-side checklist is:

1. Add `Game::<Name>` plus `to_string` / `<Name>FromString` / `allGames()` entries in `include/ccm/domain/Enums.hpp` and `src/domain/Enums.cpp`.
2. Create `include/ccm/games/<name>/<Name>SetSource.hpp` + `.cpp` implementing `ISetSource`. Mirror `MagicSetSource` / `PokemonSetSource`: expose a static `parseResponse(std::string)` helper so it's unit-testable without HTTP.
3. (Optional) Create `include/ccm/games/<name>/<Name>CardPreviewSource.hpp` + `.cpp` implementing `ICardPreviewSource`. Mirror `MagicCardPreviewSource` / `PokemonCardPreviewSource`: expose static `buildSearchUrl` + `parseResponse` helpers for unit testing without HTTP.
4. Create `include/ccm/games/<name>/<Name>GameModule.hpp` + `.cpp` implementing `IGameModule`. Pick a stable lowercase `dirName()` — it becomes the on-disk subdirectory and must never change. The module **owns** its set source and (optionally) its card preview source: override `cardPreviewSource()` to return `&previewSource_` when present (default returns `nullptr`).
5. If the game has a card type with different fields, add a `<Name>Card` domain type with hand-rolled JSON aliases. Otherwise reuse an existing one.
6. Add the new `.cpp` files to `core/CMakeLists.txt` (no glob).
7. Add tests under `tests/<name>_set_source_tests.cpp` and `tests/<name>_card_preview_source_tests.cpp` modeled on the Magic / Pokemon versions.
8. The composition root in `app/main.cpp` and the directory mapping in `app/main.cpp::dirNameForGame` must be updated too — see `app/AGENTS.md`. `CardPreviewService::registerModule(*module)` is the single registration call; modules whose `cardPreviewSource()` returns `nullptr` are silently skipped.

## Adding / changing a card-table column

When you add or rename a `tableFields` entry on a list panel (Magic or Pokemon), keep `core/`'s sort/filter helpers and their tests in lockstep:

1. Extend `MagicSortColumn` / `PokemonSortColumn` and add a `case` branch in `sortMagicCards` / `sortPokemonCards` (`core/include/ccm/services/CardSorter.hpp` + `.cpp`).
2. Add the new value-key column to the matching `matchesMagicFilter` / `matchesPokemonFilter` (`core/include/ccm/services/CardFilter.hpp` + `.cpp`) — boolean-flag columns are *excluded* (the filtering rule only checks values equivalent to JS `typeof === "string" | "number"`).
3. Add tests under `tests/card_sorter_tests.cpp` and `tests/card_filter_tests.cpp`.

## Adding a new port

1. Add the interface header under `include/ccm/ports/` with `virtual ~IFoo() = default;`.
2. Implement the adapter under `include/ccm/infra/` + `src/infra/`. Mark it `final`.
3. Update `core/CMakeLists.txt`. Wire it into the relevant service's constructor.
4. Add a fake under `tests/fakes/` modeled on `InMemoryFileSystem` and write service-level tests against it.

## Commands

Build core only: `cmake --build build --target ccm_core`
