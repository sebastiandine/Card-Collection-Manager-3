# Card Collection Manager 3

A native C++ desktop implementation
(originally a Tauri app written in Rust + TypeScript). Same JSON file layout,
same card schemas, same APIs - just a single C++ binary built on wxWidgets.

> Status: MVP. **Magic the Gathering** is fully wired end-to-end (CRUD, set
> sync from Scryfall, image management). **Pokemon TCG** has stub modules in
> place; finishing the parser is a small follow-up.

## Architecture

The project is split into three layers; dependencies only point inward:

```
app (executable / composition root)
 |- ccm_ui_wx  (wxWidgets adapter - swap to switch UI toolkit)
 |- ccm_core   (domain + services + ports + infra adapters)
```

`ccm_core` is **completely UI-agnostic**. Replacing wxWidgets with Qt or Dear
ImGui later means rewriting `ui_wx/` and reusing every line of `core/` as-is.
The only thing that crosses the boundary is `ccm::ui::AppContext`, a small
struct of references to the core services.

Inside `core/`:

- `domain/` - POD value types (`MagicCard`, `PokemonCard`, `Set`, `Configuration`,
  enums) with JSON round-trips that preserve the established serde format byte-for-byte.
- `ports/` - interfaces (`IHttpClient`, `IFileSystem`, `ICollectionRepository<T>`,
  `ISetRepository`, `IImageStore`, `IGameModule`, `ISetSource`). These are the
  seams the services depend on.
- `services/` - high-level operations that compose ports
  (`CollectionService<T>`, `SetService`, `ImageService`, `ConfigService`).
- `infra/` - concrete adapters: `CprHttpClient`, `StdFileSystem`,
  `JsonCollectionRepository<T>`, `JsonSetRepository`, `LocalImageStore`.
- `games/{magic,pokemon}/` - per-game modules. Adding a third TCG = implement
  `IGameModule` + `ISetSource`, register it in `app/main.cpp`. Done.

## Toolchain

- **Compiler**: Clang 14+ (preferred). MinGW-w64 GCC 11+ works on Windows.
  MSVC is intentionally not supported. The top-level CMake auto-selects
  Clang first; see [cmake/Toolchain.cmake](cmake/Toolchain.cmake).
- **Build system**: CMake 3.22+
- **Dependency management**: CMake `FetchContent` from upstream Git repos.
  See [cmake/Dependencies.cmake](cmake/Dependencies.cmake).

| Dependency | Version | Used for |
|---|---|---|
| nlohmann/json | v3.11.3 | All JSON serialization |
| libcpr/cpr    | 1.10.5  | HTTPS calls to Scryfall / Pokemon TCG API (libcurl built in-tree) |
| wxWidgets     | v3.2.5  | Cross-platform UI toolkit |
| doctest       | v2.4.11 | Unit tests for `ccm_core` (only when `CCM_BUILD_TESTS=ON`) |

## Build

### Windows (Clang via LLVM, recommended)

```powershell
# Pre-reqs: install LLVM (clang.exe in PATH) and CMake.
cmake -S . -B build -G "Ninja"
cmake --build build --parallel
.\build\bin\ccm.exe
```

### Windows (MinGW-w64 GCC fallback)

This is the verified path - tested against MSYS2 UCRT64 (`gcc 15.2`) and CMake
4.3:

```powershell
# Pre-reqs: MSYS2 with mingw-w64-ucrt-x86_64-gcc and CMake 3.22+ on PATH.
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
.\build\bin\ccm.exe
```

Notes:

- Configure takes ~1-3 minutes (downloads + probes wxWidgets / curl / cpr).
- A clean build is ~9 minutes on a 4-core machine; most of that is wxWidgets.
- `cpr` is built as a shared library, so `build\bin\` contains `libcpr.dll`,
  `libcurl.dll`, `libzlib.dll` next to `ccm.exe` — keep them together.
- MinGW-w64 also needs its own runtime alongside the exe or on `PATH`:
  `libgcc_s_seh-1.dll`, `libstdc++-6.dll` (from your MSYS2 UCRT64 `bin/`).
  Easiest: `$env:PATH = "P:\msys2\msys64\ucrt64\bin;$env:PATH"` before launch.

### Linux / macOS

```bash
cmake -S . -B build -G Ninja
cmake --build build --parallel
./build/bin/ccm
```

> **First build is slow** because `FetchContent` fetches and builds wxWidgets
> and curl from source. Subsequent reconfigures reuse the cache.

### Build options

| Option | Default | Effect |
|---|---|---|
| `CCM_USE_SYSTEM_WX` | `OFF` | When `ON`, `find_package(wxWidgets)` is used instead of fetching wx source. Use this for fast iteration once you have a system wx install. |
| `CCM_BUILD_TESTS`   | `ON`  | When `ON`, builds the `ccm_core_tests` doctest binary. |
| `CMAKE_BUILD_TYPE`  | `Release` | Standard CMake build type. |

Example - faster local iteration with system wx and no tests:

```bash
cmake -S . -B build -DCCM_USE_SYSTEM_WX=ON -DCCM_BUILD_TESTS=OFF
cmake --build build
```

## Running tests

```bash
cmake -S . -B build -DCCM_BUILD_TESTS=ON
cmake --build build --target ccm_core_tests
ctest --test-dir build --output-on-failure
```

The test suite covers the entire non-UI surface:

- `FsNames` (filename sanitizer + index parser)
- Domain JSON round-trips (Magic / Pokemon / Set / Configuration / enums)
- `CollectionService<T>` (add/update/remove/findById, image cleanup)
- `ConfigService` (defaults, load, store, malformed JSON)
- `JsonCollectionRepository<T>` and `JsonSetRepository` (over an in-memory FS fake)
- `SetService` (module routing, error propagation)
- `MagicSetSource::parseResponse` (Scryfall mapping, digital filter, sorting)
- `PokemonSetSource` stub behavior

UI tests are out of scope; tests run hermetically against in-memory fakes
(`tests/fakes/InMemoryFileSystem.{hpp,cpp}`) so the suite is fast and
deterministic.

## Data layout

The data layout is stable, so `collection.json` and `images/` directories remain
interchangeable with older builds:

```
<dataStorage>/
  magic/
    collection.json
    sets.json
    images/
      <set>+<name>+<idx>.{png,jpg}
  pokemon/
    ...
config.json   (next to the executable)
```

`config.json` defaults to:

```json
{ "dataStorage": "<absolute path to the folder containing ccm executable>",
  "defaultGame": "Magic" }
```

It can be overridden in **File > Settings**.

## Migrating Existing Data

The app reads existing JSON files unchanged. To move a collection:

1. Close any other running instance that uses the same data folder.
2. Copy `<legacy-data>/magic/collection.json` and `<legacy-data>/magic/images/`
   into the equivalent location under your configured data directory.
3. Launch the app, then set its data directory in **File > Settings** if needed.

## Adding a new TCG

1. Implement `ccm::IGameModule` and `ccm::ISetSource` for the new game under
   `core/include/ccm/games/<name>/` and `core/src/games/<name>/`.
2. Register the module in `app/main.cpp` and add a `Game::*` enum value plus
   string mapping in [core/include/ccm/domain/Enums.hpp](core/include/ccm/domain/Enums.hpp).
3. Map the new enum value to a directory name in `app/main.cpp::dirNameForGame`.
4. (Optional) Add a card type and a UI panel in `ui_wx/`.

The whole point of the layered design is that step 4 is the only one that
touches wxWidgets - the rest is pure logic, fully unit-testable.

## License

This project is a personal implementation. See repository licenses and dependency
licenses for details on included code and assets.
