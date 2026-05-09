# app/AGENTS.md

The `ccm` executable — composition root only. The single place where concrete adapter types are mentioned. Read the root `AGENTS.md` first.

## File pointers

- `main.cpp` — the entire app. Defines `CcmApp : public wxApp`, builds the dependency graph in `OnInit()`, then hands an `AppContext` to `MainFrame`.
- `CMakeLists.txt` — declares the `ccm` target. Sets `WIN32_EXECUTABLE TRUE` on Windows so no console window appears. Links `ccm_core`, `ccm_ui_wx`, `ccm_warnings`.

## Conventions

1. **Composition root is the only place** that names concrete adapters: `StdFileSystem`, `CprHttpClient`, `JsonCollectionRepository<MagicCard>`, `JsonCollectionRepository<PokemonCard>`, `JsonCollectionRepository<YuGiOhCard>`, `JsonSetRepository`, `LocalImageStore`, `LocalPreviewByteCache`, `MagicGameModule`, `PokemonGameModule`, `YuGiOhGameModule`, `MagicGameView`, `PokemonGameView`, `YuGiOhGameView`, etc. If a concrete adapter type appears anywhere else in the codebase, move the wiring here.
2. **Member declaration order in `CcmApp` matters** — destruction is reverse, so a member that depends on another (e.g. `magicCollSvc_` depends on `magicRepo_` and `imgStore_`; `previewSvc_` depends on `http_` and is consumed by `ctx_`; `magicView_` depends on the typed `magicCollSvc_` and the shared services) must be declared **after** its deps. Do not reorder casually.
3. **Use `std::unique_ptr` for everything owned** by `CcmApp`. The `AppContext` then holds plain references into those owned objects, plus a vector of `IGameView*` raw pointers (the `unique_ptr<>`s for the views are the actual owners; the vector just describes the active set).
4. **Game-to-directory mapping** lives in `dirNameForGame(Game)` (anonymous namespace). When adding a new game, extend this function — it is wired into all three repositories (`JsonCollectionRepository`, `JsonSetRepository`, `LocalImageStore`).
5. **`config.json` location** is the executable's parent directory, resolved via `wxStandardPaths::Get().GetExecutablePath()`. Do not change this — existing installations rely on that location.
6. **Image format handlers** must be registered via `wxImage::AddHandler(new wxPNGHandler)` and `new wxJPEGHandler` before any image is loaded. They are added in `OnInit()` first thing — keep it that way.
7. **Card preview source ownership** lives inside the `IGameModule`. The composition root never constructs an `<Name>CardPreviewSource` directly; it calls `previewSvc_->registerModule(*<name>Mod_)` and the service pulls the module's preview source via `IGameModule::cardPreviewSource()` (returning `nullptr` is silently skipped).
8. **One `CprHttpClient` per app, shared by every consumer.** The single `http_` instance is handed to `SetService`, `CardPreviewService`, and every per-game module. Do **not** construct a second `CprHttpClient` (or pass `cpr::Get(...)` directly) from anywhere — the adapter holds a long-lived `cpr::Session` whose connection cache + TLS keep-alive is what makes repeat lookups fast (game-agnostic; see `core/AGENTS.md` convention 11). The shared instance also gives `CardPreviewService`'s in-memory LRU a single source of truth to cache against.
9. **One `LocalPreviewByteCache` per app**, rooted at `<exeDir>/.cache/preview-cache/` — i.e. **next to the executable**, in the same scope as `config.json`. **Do not** root the cache at `config_->current().dataStorage`: the user's data-storage path is user-configurable at runtime and is meant for the user's collection (cards, scans, set lists). Previews are downloaded-from-network artifacts that (a) must not move when the user relocates their collection, (b) must not be uploaded/synced together with the user's data dir, and (c) must not survive a fresh install elsewhere on disk. Pinning the cache to `exeDir` is what gives those properties without writing extra plumbing for each data-storage flow. The umbrella `.cache/` directory is reserved for any future computed-from-network caches (set-list snapshots, etc.); the leading dot keeps it out of the way for users poking around the install folder. Cache updates flow entirely through cache keys: `CardPreviewService` invalidates entries automatically when the cache key changes (record edits) and rewrites them when a same-key resolution flips between positive and negative — there is no `clearCache(...)` API. To wipe the cache manually, delete `<exeDir>/.cache/`; reinstalling / moving the executable also resets the cache by design. Construct the cache after `ConfigService` (so the dependency graph is the same as before; the cache itself only needs `*fs_` and the resolved `exeDir`) and before `CardPreviewService` (so the service can hold a stable raw pointer); declare the member after `config_`/`fs_` and before `previewSvc_` to keep destruction order correct. See `core/AGENTS.md` convention 10 and `docs/caching.md` ("Updating cached entries") for the full cache shape, policy, and update mechanic.

## Required follow-ups

- After adding a new game module you **must**: (1) add a `unique_ptr<<Name>GameModule>` member in declaration-order-correct position, (2) construct it in `OnInit()`, (3) call `setSvc_->registerModule(<name>Mod_.get())`, (4) call `previewSvc_->registerModule(*<name>Mod_)` (no-op when the module has no preview source), (5) extend `dirNameForGame`, (6) add a typed `JsonCollectionRepository<<Name>Card>` + `CollectionService<<Name>Card>` if the game has a custom card type, (7) construct a `<Name>GameView` and append its raw pointer to the `AppContext::gameViews` vector, (8) make sure the view's `unique_ptr<>` member sits **after** all its deps (typed services + `IGameModule`).
- After adding a new core service you **must** add a `unique_ptr<...>` member, construct it in `OnInit()` after its deps, and add a reference field to `AppContext`.
- After adding a new dependency edge you **must** verify destruction order is still correct: deps **before** dependents in the member list.
- After changing the IGameView contract or the AppContext shape, update `docs/adding-a-new-game.md` so the canonical procedure stays in sync.

## Anti-patterns

- Don't add business logic here. If something is more than `std::make_unique` and a `register/Bind` call, it belongs in `core/`.
- Don't construct services on the stack inside `OnInit()` — they must outlive the `MainFrame`, so they live as `CcmApp` members.

## Commands

- Build the binary: `cmake --build build --target ccm`
- Run on Windows / MinGW-w64: `.\build\bin\ccm3.exe`. The cpr/curl/zlib DLLs are placed next to the exe automatically; the MSYS2 UCRT64 runtime (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`) needs to be on `PATH` (e.g. `P:\msys2\msys64\ucrt64\bin`). On verified runs the exe loads under window title "Card Collection Manager 3".
