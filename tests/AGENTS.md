# tests/AGENTS.md

`ccm_core_tests` — doctest unit tests for `ccm_core`. Hermetic, fast, no real network or disk. Read the root `AGENTS.md` first.

## File pointers

- `main.cpp` — doctest entry point with `DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN`. Do not put tests here.
- `fakes/InMemoryFileSystem.{hpp,cpp}` — `IFileSystem` implementation backed by `std::map`. Normalizes paths via `lexically_normal().generic_string()` (always `/` separators).
- `fs_names_tests.cpp` — `formatTextForFs` + `parseIndexFromFilename`. Both are compatibility ports of the original Rust path rules.
- `domain_json_tests.cpp` — JSON round-trip tests for every domain type. **Update this file whenever a domain type changes.**
- `image_service_tests.cpp` — `ImageService` (uses inline `RecordingImageStore` fake).
- `collection_service_tests.cpp` — `CollectionService<MagicCard>` (uses inline `InMemoryRepo` + `StubImageStore`).
- `config_service_tests.cpp` — `ConfigService` against `InMemoryFileSystem`.
- `json_collection_repository_tests.cpp`, `json_set_repository_tests.cpp` — repository round-trips against `InMemoryFileSystem`.
- `local_image_store_tests.cpp` — `LocalImageStore` against `InMemoryFileSystem` + `ConfigService`: `copyIn` (extension preserved, missing source errors), `remove` (existing file deleted; absent path is a no-op), `resolvePath` layout under `dataStorage/<game>/images/`.
- `set_service_tests.cpp` — `SetService` with `FakeSetSource` + `InMemSetRepo`.
- `magic_set_source_tests.cpp` — `MagicSetSource::parseResponse` (Scryfall mapping). Drives `fetchAll` via `FixedHttpClient` fake.
- `magic_card_preview_source_tests.cpp` — `MagicCardPreviewSource::buildSearchUrl` URL-encoding rules + `parseResponse` (`data[0].image_uris.normal`). Drives `fetchImageUrl` via `FixedHttpClient`.
- `card_preview_service_tests.cpp` — `CardPreviewService` registry/orchestration through `registerModule(IGameModule&)` with an inline `FakeGameModule` returning a `FakeSource : ICardPreviewSource` (which carries a `PreviewLookupError::Kind` knob so tests can drive both transient and not-found paths) and a `FixedHttpClient`. Both fakes count `calls` so cache-hit assertions are precise. Pin-downs include: "module returning nullptr is silently skipped", the per-game `detectFirstPrint` / `detectPrintVariants` opt-in guards, and the LRU bytes cache (repeat `fetchPreviewBytes` for the same `(game, name, setId, setNo)` returns the cached payload without touching the source or HTTP; different cards get separate cache slots; transient errors are **not** cached so a flaky connection recovers; `fetchImageBytesByUrl` is keyed by URL and serves the per-game card-back fallback from the same LRU). Production `fetchAndCache` rejects empty HTTP bodies (not exercised by these fakes unless a test sets an empty `body` deliberately). The negative-cache behavior is also pinned down: a `NotFound` source error writes through to the persistent cache *and* short-circuits the next lookup (source not re-invoked); editing a lookup-relevant field invalidates the negative entry automatically; warm-restart (a fresh service over the same cache fake) honors a previously stored negative entry; and a later positive result for the same key replaces the negative entry. The persistent-tier wiring uses an inline `InMemoryByteCache : IPreviewByteCache` fake whose `Entry { negative, payload }` carries the kind explicitly.
- `local_preview_byte_cache_tests.cpp` — `LocalPreviewByteCache` adapter against `StdFileSystem` (the only test in the suite that touches real disk; each case scopes itself to a unique `temp_directory_path()/ccm_preview_cache_test_*` directory and cleans up via an RAII `TempDir`). Pin-downs: store/load round-trips bytes verbatim; missing key is a clean miss; empty payload is silently skipped; sidecar mismatch (faked hash collision) is treated as a miss so we never serve the wrong card's bytes (or wrong card's negative verdict); the cache survives an adapter restart over the same directory; total-size eviction drops the oldest `.bin` by mtime when a `store` would exceed the cap; a `load` touches the entry's mtime so frequently-viewed cards survive eviction. Negative-entry coverage: `storeNegative` round-trips as `NegativeHit` (not a miss, not a payload, and not counted against the byte cap); negatives survive an adapter restart; a later positive `store` overwrites a previous negative and a later `storeNegative` overwrites a previous positive (releasing its bytes from the cap); and the sidecar collision check applies to negative entries too.
- `std_file_system_tests.cpp` — `StdFileSystem` directly (`exists`, `isDirectory`, `ensureDirectory`, `readText`, `writeText`, `copyFile`, `remove`, `listDirectory`) under a unique `temp_directory_path()/ccm_std_fs_test_*` directory per case; scope matches the real-disk exception documented for preview-cache tests.
- `pokemon_set_source_tests.cpp` — `PokemonSetSource::parseResponse` (api.pokemontcg.io/v2/sets shape — `data[].id`, `name`, `releaseDate` already in `YYYY/MM/DD`) + sort-by-release-date stability. Drives `fetchAll` via `FixedHttpClient` and asserts the public endpoint URL.
- `pokemon_card_preview_source_tests.cpp` — `PokemonCardPreviewSource::buildSearchUrl` (percent-encoded `name:` / `set.id:` / `number:` triple, with collector-number `4/102` -> `4` normalization) + `parseResponse` (`data[0].images.large` with `images.small` fallback). Drives `fetchImageUrl` via `FixedHttpClient`.
- `yugioh_set_source_tests.cpp` — `YuGiOhSetSource::parseResponse` for YGOPRODeck `cardsets.php` (`set_code`, `set_name`, `tcg_date`) including `YYYY-MM-DD` -> `YYYY/MM/DD` rewrite and chronological sort checks.
- `yugioh_card_preview_source_tests.cpp` — `YuGiOhCardPreviewSource` Yugipedia + YGOPRODeck unit coverage. Helper-level tests pin down `normalizeName` (whitespace + Yugipedia-policy punctuation stripping), `ygoRarityShortCode` + `rarityCodeFor` (CCM3 dialog rarity names → canonical short codes used by both the YGO overview table and Yugipedia filename generation; unknown rarity falls through), `extractSetCode` (`LOB-005` / `LOB-DE005` → `LOB`), `buildCandidateFilenames` (printed-edition first, EN/NA/EU/AU + png/jpg, rarity-less fallback round, empty list when slug or set code is missing), `buildYugipediaQueryUrl` (single `titles=File:A|File:B` batch, percent-encoded), and `parseYugipediaResponse` (returns the URL of the highest-priority filename that resolved, errors when every candidate is `missing`). End-to-end `fetchImageUrl` cases use a `RoutingHttpClient` to verify Yugipedia is queried first and the per-printing scan is returned when found, that empty/error Yugipedia responses fall through to the YGOPRODeck `card_images[0]` fallback, that the YGOPRODeck error is propagated when both upstreams fail, and that an empty `setNo` skips Yugipedia entirely. `parseFirstPrint` preferred-`set_name` lookup is also covered for the auto-detect path. `parsePrintVariants` includes synthetic scenarios aligned with the `yugioh_same_card_set_variant_tests` fixture (dual-rarity vs multi-code within one display set, duplicate suppression, and no merge across unrelated `set_name` rows when the picker label matches nothing).
- `card_sorter_tests.cpp` — `sortMagicCards` / `sortPokemonCards` per-column behavior. Pin-down tests for `byField`-equivalent semantics: case-insensitive strings, chronological set sort via `set.releaseDate`, numeric `amount`, `false < true` boolean order, stable composition (sort by name then by set keeps inner-name order). Update this file whenever you add a new column / sort key.
- `card_filter_tests.cpp` — `matchesMagicFilter` / `matchesPokemonFilter` / `matchesYuGiOhFilter` row-matcher behavior. Pin-down tests for `applyFilter`-equivalent semantics: case-insensitive substring match across `tableFields` valueKeys (name, set.name, language, condition, amount-as-string, note; Pokemon adds `setNo`; Yu-Gi-Oh adds `setNo` + `rarity`), boolean flag columns intentionally excluded, empty filter matches everything. Update this file whenever you add a new searchable column.
- `CMakeLists.txt` — explicit list of every `.cpp` (no glob).

## Conventions

1. **Framework**: doctest. Each test file `#include <doctest/doctest.h>` and uses `TEST_SUITE("...")` + `TEST_CASE("...")`. Asserts: `CHECK`, `REQUIRE`, `CHECK_THROWS`.
2. **No real I/O.** Everything goes through `ccm::testing::InMemoryFileSystem` or an inline test-local fake. If you need HTTP, write a fake `IHttpClient` like `FixedHttpClient` in `magic_set_source_tests.cpp`. **Narrow exceptions** (unique temp dirs + RAII cleanup): `local_preview_byte_cache_tests.cpp` (mtime/size semantics tied to real `std::filesystem`) and `std_file_system_tests.cpp` (`StdFileSystem` integration). Do not add further real-disk suites without the same cleanup guarantees.
3. **Fakes for narrow concerns stay in the test file** as anonymous-namespace classes (e.g. `RecordingImageStore`, `InMemoryRepo`). Promote a fake to `tests/fakes/` only when more than one test file needs it.
4. **Path strings** in expectations must use forward slashes. The fake normalizes everything to `generic_string()`. Do not hard-code `\` separators.
5. **Test names** describe behavior, not implementation. Prefer "missing file is created with defaults" over "test_init_no_file".
6. **Add a `.cpp` to the `add_executable` call in `tests/CMakeLists.txt`.** No glob.

## Required follow-ups

- After modifying any domain type field or alias you **must** extend the matching test in `domain_json_tests.cpp`.
- After modifying `formatTextForFs` or `parseIndexFromFilename` you **must** extend `fs_names_tests.cpp` — these are byte-compatibility shims with the original Rust code.
- After adding a new service in `core/` you **must** add a corresponding `<name>_service_tests.cpp` with at least the happy-path and one error-path test.
- After adding a new game's set source / card preview source you **must** add `tests/<name>_set_source_tests.cpp` and (if applicable) `tests/<name>_card_preview_source_tests.cpp` mirroring the Magic and Pokemon files. Add them to `tests/CMakeLists.txt`.
- After adding or changing `ICardPreviewSource` optional capabilities (for example `detectFirstPrint` / `detectPrintVariants`), you **must** extend `card_preview_service_tests.cpp` and the corresponding game source tests to cover both supported and unsupported paths.

## Commands

- Configure with tests on:  
  `cmake -S . -B build -DCCM_BUILD_TESTS=ON`
- Build the suite:  
  `cmake --build build --target ccm_core_tests`
- Run all tests:  
  `ctest --test-dir build --output-on-failure`
- Run a single test by name pattern:  
  `./build/bin/ccm_core_tests --test-case="*nextImageIndex*"`
- Run a single suite:  
  `./build/bin/ccm_core_tests --test-suite="MagicSetSource::parseResponse"`

## Anti-patterns

- Don't depend on `ccm_ui_wx` from tests. UI is out of scope here.
- Don't rely on file paths existing on the host (no `/tmp`, no `C:\Users\...`). Use `InMemoryFileSystem`.
- Don't add tests that require network access. The Pokemon stub test checks the message, not a real API call.
