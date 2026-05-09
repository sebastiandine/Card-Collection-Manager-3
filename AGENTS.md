# AGENTS.md

C++ desktop implementation (originally based on a Tauri Rust+TS version) — single wxWidgets binary built with CMake + FetchContent.

## Project structure

- `core/` — `ccm_core` static library. UI-agnostic domain, ports, services, infra adapters. **Never** depends on wxWidgets. See `core/AGENTS.md`.
- `ui_wx/` — `ccm_ui_wx` static library. The only place that touches wxWidgets. See `ui_wx/AGENTS.md`.
- `app/` — `ccm` executable (composition root). Wires concrete adapters into services. See `app/AGENTS.md`.
- `tests/` — `ccm_core_tests` doctest binary. Pure-logic tests against in-memory fakes. See `tests/AGENTS.md`.
- `docs/` — long-form developer documentation. Start with `docs/adding-a-new-game.md` for the canonical end-to-end procedure for extending the app with a new TCG. See `docs/AGENTS.md`.
- `.github/workflows/` — GitHub Actions CI/release workflows. See `.github/workflows/AGENTS.md` for orchestrator/reusable workflow rules and CI invariants.
- `cmake/` — `Toolchain.cmake` (Clang first, MinGW-w64 fallback), `Dependencies.cmake` (FetchContent pins), `CompilerWarnings.cmake` (`ccm_warnings` interface target).
- `CMakeLists.txt` — top-level. Defines options `CCM_USE_SYSTEM_WX` (default OFF) and `CCM_BUILD_TESTS` (default ON).
  - Build metadata option: `CCM_APP_VERSION` (defaults to `${PROJECT_VERSION} (localbuild)` for local/manual builds, overridden by CI).

## Architecture rules (do not break)

1. Dependencies point inward only: `app` -> `ui_wx` -> `core`. `core` depends on no other first-party target.
2. `core` must not include any wx header. CI-equivalent: `rg "wx/" core/` must return zero hits.
3. Cross-boundary types are domain types and `ccm::ui::AppContext`. UI code consumes services via the references in `AppContext` — **never** by including a concrete adapter header.
4. Errors cross port boundaries as `ccm::Result<T, E=std::string>` (see `core/include/ccm/util/Result.hpp`). Do not throw across ports; reserve exceptions for genuinely unrecoverable bugs.
5. JSON layout must stay byte-for-byte stable: aliases `releaseDate`, `setNo`, `firstEdition`, `dataStorage`, `defaultGame`, and the `signed` JSON key (mapped to C++ field `signed_`). If you touch a domain type, update the round-trip test in `tests/domain_json_tests.cpp`.

## Toolchain

- **Compilers**: Clang 14+ preferred, MinGW-w64 GCC 11+ fallback on Windows. **Do not** add MSVC support.
- **C++ standard**: C++20 (`CMAKE_CXX_STANDARD 20`, `CXX_EXTENSIONS OFF`).
- **Build system**: CMake 3.22+ with `FetchContent`. Pin every dep by tag in `cmake/Dependencies.cmake`; never use `master`.

## Key dependencies

| Library | Version | Purpose |
|---|---|---|
| nlohmann/json | v3.11.3 | All JSON serde |
| libcpr/cpr | 1.10.5 | HTTPS (libcurl built in-tree, Schannel on Windows) |
| wxWidgets | v3.2.5 | UI toolkit (only `ui_wx/` may use it) |
| doctest | v2.4.11 | Tests (only when `CCM_BUILD_TESTS=ON`) |

## Commands

Run from the **workspace root**.

- Configure (Clang/Ninja, FetchContent wx):  
  `cmake -S . -B build -G Ninja`
- Configure (Windows MinGW-w64 fallback — verified working with MSYS2 UCRT64 GCC 15.2 + CMake 4.x):  
  `cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release`
- Configure with system wx for fast iteration:  
  `cmake -S . -B build -G Ninja -DCCM_USE_SYSTEM_WX=ON`
- Build everything:  
  `cmake --build build --parallel`
- Run the app:  
 `./build/bin/ccm3` (`.\build\bin\ccm3.exe` on Windows)
- Run tests (CCM_BUILD_TESTS defaults to ON):  
  `ctest --test-dir build --output-on-failure` — current baseline: **164 tests, all green**.
- Build tests only:  
  `cmake --build build --target ccm_core_tests`

> **Windows runtime note**: `cpr` is built as a shared library, so `build/bin/` ends up with `libcpr.dll`, `libcurl.dll`, `libzlib.dll` next to `ccm3.exe`. With MinGW-w64 you also need `libgcc_s_seh-1.dll` and `libstdc++-6.dll` from your MSYS2 UCRT64 `bin/` on `PATH` (or copied alongside the exe) to launch from Explorer.
>
> **Windows rebuild note**: linking `ccm3.exe` fails with `Permission denied` if the app is still running/locked. Close `ccm3.exe` before rebuilding app targets.
>
> **Windows cold-start note**: first launch right after a fresh build is often slower than subsequent launches due to cold file cache and Windows security scanning (Defender/SmartScreen) on the new exe/dll set. Warm launches are the meaningful baseline for app-side perf changes.

## UI performance guardrails

- Keep first paint responsive: avoid heavy synchronous work in constructors of top-level windows/dialogs.
- For startup, defer non-critical work with `CallAfter(...)` so the frame appears before data loading.
- Preserve "select first row on startup" behavior without blocking first paint by scheduling the initial selection with `CallAfter(...)` instead of selecting synchronously during row rebuild.
- Avoid repeated set-list loads when opening Add/Edit: cache Magic sets in `MainFrame` and reuse them in `CardEditDialog`.
- Pass preloaded set data to dialogs by pointer/reference, not by value, to avoid copying large vectors on every open.
- While constructing/populating dialogs with many controls/choices, wrap with `Freeze()`/`Thaw()` and append choice items in bulk (`wxArrayString`) to reduce layout/repaint churn.
- Keep selected-card preview usable when remote lookup fails: show a per-game card-back fallback image (CCM2 parity), not a blank preview panel.
- Card preview round-trips are slow (HTTPS handshake + image GET, often two hosts). The three amortizations in place — all game-agnostic — must stay. The full update mechanic (key-driven invalidation, positive↔negative same-key replacement, eviction, manual cache clearing) is documented in `docs/caching.md` → "Updating cached entries"; do **not** add a side-channel `clearCache(...)` API to `CardPreviewService` — keep updates flowing through cache keys so the in-memory and disk tiers stay aligned automatically.
  - `CardPreviewService` keeps a bounded in-memory LRU (`kCacheCapacity`) of preview bytes keyed by `(game, name, setId, setNo)` plus a by-URL cache for the per-game card-back fallback. Re-selecting a row already viewed in this session is decode-only, no HTTP. Source failures are split by `PreviewLookupError::Kind`: `NotFound` (the upstream answered cleanly that the record has no image) is **negative-cached** so subsequent clicks short-circuit to the card-back placeholder without HTTP, while `Transient` (HTTP/network/parse) is **never** cached so a brief outage can recover on the next selection. Editing a lookup-relevant field changes the cache key and invalidates the negative entry automatically.
  - `LocalPreviewByteCache` (port `IPreviewByteCache`) extends the LRU with an on-disk byte cache rooted at `<exeDir>/.cache/preview-cache/` — **next to the executable, in the same scope as `config.json`, NOT inside the user-configurable `dataStorage` path** so previews don't follow the user's collection when the data-storage path is reconfigured (the umbrella `.cache/` directory is reserved for any future computed-from-network caches). Both positive previews and `NotFound` verdicts **survive app restarts**. Lookup order is memory → disk → source/HTTP; a disk hit (positive or negative) is promoted into the in-memory tier so the follow-up call stays decode-only. Total `.bin` payload size is capped (default 64 MiB) and oldest-by-mtime entries are evicted when a new write would exceed the cap; tiny `.neg` markers are not counted against the cap. The persistent tier is fire-and-forget: any I/O error is swallowed by the adapter so disk problems can never break the preview path.
  - `CprHttpClient` owns a single long-lived `cpr::Session` (and therefore a single libcurl easy handle) with keep-alive enabled, so repeat HTTPS calls to the same host (`api.scryfall.com`, `api.pokemontcg.io`, `db.ygoprodeck.com`, `yugipedia.com`, `ms.yugipedia.com`) reuse the existing TLS connection. Concurrent callers are serialized through a mutex — easy handles are not thread-safe and the preview path is single-flight already.

## Windows UI theming guardrails

- `wxWidgets` native dark-mode behavior on Windows is inconsistent across controls and OS builds; prefer explicit app theming in `ui_wx/src/Theme.cpp` plus targeted native hints only where needed.
- Treat UI text from domain/services as UTF-8 and convert explicitly at wx boundaries (`wxString::FromUTF8(...)` for display, `ToStdString(wxConvUTF8)` for write-back); do not rely on implicit `std::string` conversions on Windows.
- For dialogs (`wxDialog`) and frames (`wxFrame`), apply title-bar dark mode through top-level-window handling (not frame-only handling), otherwise modal window headers stay light.
- The `wxListCtrl` native header can ignore dark hints; if native theming is unreliable, use a custom themed header row and preserve key UX parity (single-click sort, edge-drag resize, divider double-click autosize).
- Do **not** apply `Explorer` class theming to `wxTextCtrl` in dark mode; some Windows builds force black typed text. Keep edit controls palette-driven, and for critical fields (for example the top-right filter box) enforce colors through `WM_CTLCOLOREDIT` handling in `MainFrame` when needed.
- Theme modal dialogs explicitly before `ShowModal()` (Settings, Create/Edit, image viewer, etc.) so they don't inherit mismatched defaults from Windows.
- For button hover/pressed contrast fixes in dark theme, prefer explicit state handling in `Theme.cpp`; native Windows button states can override wx colors and produce unreadable white-on-white combinations.
- Keep button theming state dynamic across theme switches (Dark <-> Light). Avoid lambdas that permanently capture old theme colors or behavior; stale handlers can make light-mode buttons look wrong.
- After changing `ui_wx` theming behavior, rebuild the final app target (`cmake --build build --target ccm --parallel`), not just `ccm_ui_wx`, before validating runtime behavior.
- If linker fails with `Permission denied` on `build/bin/ccm3.exe`, the app is still running; close it before rebuilding.

## Required follow-ups

- After modifying a domain type's fields or JSON layout you **must** update the matching round-trip test in `tests/domain_json_tests.cpp` and re-run tests.
- After adding a new `.cpp` to `core/` or `ui_wx/` you **must** add it to that package's `CMakeLists.txt`. There is no glob.
- After adding a new dependency you **must** verify its license is compatible with this repository's MIT license before merging.
- After changing SonarQube coverage generation, keep dependency build outputs excluded at gcov discovery time (for example `gcovr --exclude-directories "build/_deps"`); output-only excludes are not enough for third-party `.gcda` files.
- After adding a new game module you **must**: (1) extend `Game` enum + string mappings in `core/include/ccm/domain/Enums.hpp`, (2) register the module in `app/main.cpp`, (3) add a directory mapping in `app/main.cpp::dirNameForGame`, (4) implement an `IGameView` derived class (or `<Name>GameView`) and add it to `AppContext::gameViews` in the composition root.
- After changing the per-game seams (`IGameModule`, `IGameView`, the `BaseCard*Panel` template hooks) you **must** update `docs/adding-a-new-game.md` so the canonical "add a new game" walkthrough stays in sync with the code.
- After changing `formatTextForFs` or `parseIndexFromFilename` you **must** update `tests/fs_names_tests.cpp` — these functions exist to stay byte-compatible with the original Rust `util/fs.rs`.

## Anti-patterns

- Don't include `wx/...` headers from `core/` (breaks layering and tests will refuse to build).
- Don't add tests that hit real network or real disk; use the fake `ccm::testing::InMemoryFileSystem` and the existing http/source fakes.
- Don't enable `-Wconversion` / `-Wsign-conversion`; they fight wxWidgets's `int` IDs. They were intentionally removed from `cmake/CompilerWarnings.cmake`.
- Don't use `master` for FetchContent tags. Bump deliberately.
- Don't bump `cpr` past `1.10.5` without re-doing the curl install/export plumbing: cpr 1.11.x adds `install(EXPORT cprTargets)` rules that reference `libcurl_shared`, which isn't in any export set when curl is built as a sub-project, breaking configure. The 1.10.5 + `HAVE_IOCTLSOCKET_FIONBIO=ON` workaround in `cmake/Dependencies.cmake` is the verified MinGW-w64 path — do not remove it without an end-to-end Windows build first.
- Don't create multiple top-level triggers for the same CI intent (feature or master). Keep one triggered orchestrator workflow (`feature-ci.yml`, `master-ci.yml`) and use `workflow_call` reusable workflows for OS-specific splits so GitHub Actions stays a single run per intent.
