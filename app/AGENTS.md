# app/AGENTS.md

The `ccm` executable — composition root only. The single place where concrete adapter types are mentioned. Read the root `AGENTS.md` first.

## File pointers

- `main.cpp` — the entire app. Defines `CcmApp : public wxApp`, builds the dependency graph in `OnInit()`, then hands an `AppContext` to `MainFrame`.
- `CMakeLists.txt` — declares the `ccm` target. Sets `WIN32_EXECUTABLE TRUE` on Windows so no console window appears. Links `ccm_core`, `ccm_ui_wx`, `ccm_warnings`.

## Conventions

1. **Composition root is the only place** that names concrete adapters: `StdFileSystem`, `CprHttpClient`, `JsonCollectionRepository<MagicCard>`, `JsonSetRepository`, `LocalImageStore`, `MagicCardPreviewSource`, etc. If a concrete adapter type appears anywhere else in the codebase, move the wiring here.
2. **Member declaration order in `CcmApp` matters** — destruction is reverse, so a member that depends on another (e.g. `magicCollSvc_` depends on `magicRepo_` and `imgStore_`; `previewSvc_` depends on `http_` and is consumed by `ctx_`) must be declared **after** its deps. Do not reorder casually.
3. **Use `std::unique_ptr` for everything owned** by `CcmApp`. The `AppContext` then holds plain references into those owned objects.
4. **Game-to-directory mapping** lives in `dirNameForGame(Game)` (anonymous namespace). When adding a new game, extend this function — it is wired into all three repositories (`JsonCollectionRepository`, `JsonSetRepository`, `LocalImageStore`).
5. **`config.json` location** is the executable's parent directory, resolved via `wxStandardPaths::Get().GetExecutablePath()`. Do not change this — existing installations rely on that location.
6. **Image format handlers** must be registered via `wxImage::AddHandler(new wxPNGHandler)` and `new wxJPEGHandler` before any image is loaded. They are added in `OnInit()` first thing — keep it that way.

## Required follow-ups

- After adding a new game module you **must**: (1) add a `unique_ptr<<Name>GameModule>` member in declaration-order-correct position, (2) construct it in `OnInit()`, (3) call `setSvc_->registerModule(<name>Mod_.get())`, (4) extend `dirNameForGame`, (5) add the new game's `IGameModule&` field to `AppContext` and pass it through, (6) if the game has a card preview source, construct the `<Name>CardPreviewSource` and call `previewSvc_->registerSource(Game::<Name>, *<name>PrevSrc_)`.
- After adding a new core service you **must** add a `unique_ptr<...>` member, construct it in `OnInit()` after its deps, and add a reference field to `AppContext`.
- After adding a new dependency edge you **must** verify destruction order is still correct: deps **before** dependents in the member list.

## Anti-patterns

- Don't add business logic here. If something is more than `std::make_unique` and a `register/Bind` call, it belongs in `core/`.
- Don't construct services on the stack inside `OnInit()` — they must outlive the `MainFrame`, so they live as `CcmApp` members.

## Commands

- Build the binary: `cmake --build build --target ccm`
- Run on Windows / MinGW-w64: `.\build\bin\ccm.exe`. The cpr/curl/zlib DLLs are placed next to the exe automatically; the MSYS2 UCRT64 runtime (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`) needs to be on `PATH` (e.g. `P:\msys2\msys64\ucrt64\bin`). On verified runs the exe loads under window title "Card Collection Manager 3".
