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

## Build and local development

The complete local build guide (Windows + Linux), dependency management details,
build options, runtime notes, and test commands now live in:

- [`docs/dow-doc-build-locally.md`](docs/dow-doc-build-locally.md)

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
{ "dataStorage": "<user home>/ccm3-data",
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
