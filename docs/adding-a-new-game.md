# Adding a new game to Card Collection Manager

This is the canonical, end-to-end walkthrough for extending Card Collection Manager with support for a brand-new TCG. It covers everything from picking the right external APIs to wiring the new game into the composition root and writing tests, and is written so that you should be able to follow it from top to bottom and end up with a working game on `main`.

The guide is **prescriptive about file locations and seam shapes** but generic about the game itself. Wherever you see `<Name>` substitute the lowercase game key (the on-disk directory name and the `dirName()` return value) or, where capitalization is needed, the user-visible display name. Both Magic the Gathering and Pokemon TCG follow this exact procedure — when in doubt, read the matching files under `core/include/ccm/games/magic/`, `core/include/ccm/games/pokemon/`, `ui_wx/src/Magic*.cpp` and `ui_wx/src/Pokemon*.cpp` for working references.

Read the root `AGENTS.md`, `core/AGENTS.md`, `ui_wx/AGENTS.md`, `app/AGENTS.md`, and `tests/AGENTS.md` before starting. They define the architecture rules this guide is built on top of.

---

## 1. Prerequisites: pick your external resources

Before you write any C++, gather the following. The further along you discover that something is missing, the more work you throw away.

### 1.1 Set list API (required)

A public HTTP endpoint that returns the canonical set / expansion list for the game, with at least:

- a stable identifier (`id`) — used as the on-disk and JSON key. Must be stable across API revisions.
- a human-readable name.
- a release date — used to sort the set picker chronologically.

The endpoint must be callable without authentication, or you must accept a hard-coded API key (we do not currently expose a way to ask the user for one). It must support HTTPS. Plan for the response body to be JSON; we do not have an XML or CSV path.

The release date may be in any format **as long as you can rewrite it to `YYYY/MM/DD` during parsing**, because the `Set` domain type stores it that way (see `core/include/ccm/domain/Set.hpp`) and the rest of the code assumes lexicographic comparison sorts chronologically.

### 1.2 Card preview API (optional)

A public HTTP endpoint that returns the URL of a card's preview image given some lookup key (typically `name`, `set id`, and possibly a printed collector number). If the game does not expose one, the UI will simply skip the remote preview and only show locally-stored images — the implementation is allowed to omit this seam entirely.

A few traps to plan around now, before you write code:

- **Lookup precision.** Some APIs return many ambiguous matches when you query by name only and require the set id (and sometimes the collector number) to disambiguate. Decide up front which fields make a search reliable enough to take the first result.
- **URL encoding.** All query strings must be RFC 3986 percent-encoded before they reach `IHttpClient::get` (`cpr::Url` does **not** re-encode). The Magic/Pokemon implementations have a private `urlEncode` helper you can copy.
- **Collector-number normalization.** Pokemon stores `4/102` but the API only accepts `4`. Whichever convention your domain type uses, normalize it inside `buildSearchUrl` so the wire format is whatever the API actually expects. Mismatches here produce empty result sets, which then look identical to "no preview available" and are very tedious to debug.

### 1.3 Flag icons

Identify any boolean flag columns the game needs (Magic: `Foil`, `Signed`, `Altered`; Pokemon: `Holo`, `1. Edition`, `Signed`, `Altered`). For each one that doesn't already exist, plan an SVG glyph. SVG art with a single fillable path works best — see `ui_wx/src/SvgIcons.cpp` for the established style. Re-use existing glyphs across games where the meaning is identical (`Signed` and `Altered` are shared between Magic and Pokemon).

### 1.4 Domain shape decision

Decide whether the new game can re-use an existing card type or needs its own. Re-use is allowed when **every** field has identical semantics; in practice, every game we have shipped has needed its own type because at least one flag or extra column differs (e.g. Pokemon adds `setNo`, `holo`, `firstEdition`).

If you create a new type, freeze the JSON layout now. The root `AGENTS.md` rule is unambiguous: **JSON layout must stay byte-for-byte stable** once you ship. Pick names that match any pre-existing on-disk format (this app may inherit data from a previous tool), and decide which C++ field names need a JSON alias (the canonical example: the C++ field `signed_` maps to the JSON key `"signed"` because `signed` is a C++ keyword).

---

## 2. Core: domain types and the `Game` enum

Everything below this point assumes you have already gathered the resources from §1.

### 2.1 Extend the `Game` enum

Edit `core/include/ccm/domain/Enums.hpp`:

- Add a new enumerator to `enum class Game`.
- Update the size of `allGames()` (`std::array<Game, N>`).

Edit `core/src/domain/Enums.cpp`:

- Add a `case` to `to_string(Game)`.
- Add a branch to `gameFromString(std::string_view)`.
- Add the new enumerator to the `allGames()` constexpr array.

The `Game` enum is the only place in `core/` that hardcodes which games exist. Adding a new entry here is what makes the rest of the registries (`SetService`, `CardPreviewService`, `AppContext::gameViews`, `dirNameForGame`) accept it.

### 2.2 Add the card domain type (only if needed)

If you decided in §1.4 that an existing card type fits, skip this section.

Otherwise, create `core/include/ccm/domain/<Name>Card.hpp`:

- A `struct <Name>Card` with `id`, `amount`, `name`, `set`, `note`, `images`, `language`, `condition` (these seven fields are required — the UI templates assume them) and any game-specific extras.
- Forward-declare `to_json` and `from_json` for `nlohmann::json`.
- Add a defaulted `friend bool operator==(const <Name>Card&, const <Name>Card&) = default;` so the JSON round-trip test can compare values.

Then create `core/src/domain/<Name>Card.cpp`:

- Hand-roll `to_json` and `from_json` using `nlohmann::json`. **Do not** use `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE` — the explicit form keeps JSON aliases visible and makes future stability bugs easier to catch in code review.

A real example to cargo-cult from is `core/src/domain/PokemonCard.cpp`. Note how `signed` (the JSON key) maps to `signed_` (the C++ field), and how every JSON key is spelled out. If your game uses a printed collector number, follow Pokemon's lead and store it as `setNo` (string) so the format `"4/102"` can survive a round-trip even when the API only consumes `"4"`.

### 2.3 Update the JSON round-trip test

Open `tests/domain_json_tests.cpp`. Add a `TEST_CASE` that:

1. Constructs a `<Name>Card` with **every** field non-default (including `set.id`, `set.name`, `set.releaseDate`).
2. Serializes it to `nlohmann::json`.
3. Asserts the JSON contains the expected keys with the expected literal spellings (in particular, any C++/JSON aliases like `"signed"`).
4. Round-trips it back into a `<Name>Card` and `CHECK(roundTripped == original)`.

This is the **byte-for-byte stability gate**. If you skip it, the alias bugs only surface in production after you ship and someone's collection.json fails to parse.

---

## 3. Core: set source and (optional) card preview source

### 3.1 `<Name>SetSource`

Mirrors `core/include/ccm/games/pokemon/PokemonSetSource.hpp` and the matching `.cpp`. Put your files at:

- `core/include/ccm/games/<name>/<Name>SetSource.hpp`
- `core/src/games/<name>/<Name>SetSource.cpp`

The header should declare:

- `class <Name>SetSource final : public ISetSource`
- `static constexpr const char* kEndpoint = "<your full HTTPS URL>";`
- `explicit <Name>SetSource(IHttpClient& http);`
- `Result<std::vector<Set>> fetchAll() override;`
- `static Result<std::vector<Set>> parseResponse(const std::string& body);`

The static `parseResponse` is mandatory. It is the seam you unit-test (no HTTP, no fakes — just a string in, a `Result` out). `fetchAll()` is a thin wrapper that calls `http_.get(kEndpoint)` and forwards to `parseResponse` on success.

In `parseResponse`:

1. Parse the body with `nlohmann::json::parse(body)` inside a `try`/`catch (const std::exception&)` block. Throwing across the port boundary is forbidden; catch and return `Result<std::vector<Set>>::err(...)` with a useful message.
2. Walk the response, mapping each entry to a `Set { id, name, releaseDate }`. Rewrite the release date to `YYYY/MM/DD` if the API uses a different format.
3. `std::sort` ascending by release date.
4. Return `Result<std::vector<Set>>::ok(std::move(out))`.

A note on quirky APIs: some endpoints return a top-level array, some wrap it in `{ "data": [...] }`, and some put it under a different key. The two reference implementations diverge on exactly this: Magic walks the Scryfall-shaped response, Pokemon walks `data[]`. Do whatever your API requires; it is fine for `parseResponse` to be game-specific.

### 3.2 `<Name>CardPreviewSource` (optional)

Skip this section if the game has no remote preview API.

Mirror `core/include/ccm/games/pokemon/PokemonCardPreviewSource.hpp`. The header should declare:

- `class <Name>CardPreviewSource final : public ICardPreviewSource`
- `explicit <Name>CardPreviewSource(IHttpClient& http);`
- `Result<std::string> fetchImageUrl(std::string_view name, std::string_view setId, std::string_view setNo) override;`
- `static std::string buildSearchUrl(std::string_view name, std::string_view setId, std::string_view setNo);`
- `static Result<std::string> parseResponse(const std::string& body);`

Both `buildSearchUrl` and `parseResponse` are static and pure on purpose: every URL-encoding and JSON-shape rule is testable without HTTP. Common edge cases your tests must cover:

- Names with spaces, punctuation, or non-ASCII characters (percent-encoding correctness).
- An empty `setId` (don't append the `set.id:` clause).
- An empty `setNo`, and a `setNo` that needs normalization (strip everything after `/`, strip leading zeros, etc.).
- Response with the preferred image variant present.
- Response with only the fallback image variant present.
- Empty `data[]` array.
- Malformed JSON (parse error path).

`fetchImageUrl` is a thin wrapper: build URL → `http_.get(url)` → `parseResponse(body)`.

### 3.3 `<Name>GameModule`

Wire the two sources together. Create:

- `core/include/ccm/games/<name>/<Name>GameModule.hpp`
- `core/src/games/<name>/<Name>GameModule.cpp`

Header:

```cpp
#pragma once

#include "ccm/games/IGameModule.hpp"
#include "ccm/games/<name>/<Name>CardPreviewSource.hpp"  // omit if no preview
#include "ccm/games/<name>/<Name>SetSource.hpp"

namespace ccm {

class <Name>GameModule final : public IGameModule {
public:
    explicit <Name>GameModule(IHttpClient& http);

    [[nodiscard]] Game        id() const noexcept override { return Game::<Name>; }
    [[nodiscard]] std::string dirName()     const override { return "<name>"; }
    [[nodiscard]] std::string displayName() const override { return "<Display>"; }

    ISetSource&         setSource() override { return setSource_; }
    // Omit the override below if the game has no remote preview API.
    ICardPreviewSource* cardPreviewSource() noexcept override { return &previewSource_; }

private:
    <Name>SetSource         setSource_;
    <Name>CardPreviewSource previewSource_;
};

}  // namespace ccm
```

Two subtle requirements:

- `dirName()` returns the **on-disk directory name**. Once you ship, this is forever — changing it later orphans every existing user's data. Pick something lowercase, ASCII, and short.
- `cardPreviewSource()` defaults to `nullptr` in `IGameModule`. Only override it if you actually have a preview source. Returning `nullptr` makes `CardPreviewService::registerModule(*module)` a silent no-op for that game; the UI gracefully falls back to "no preview available".

The `.cpp` is one line of constructor body — see `core/src/games/pokemon/PokemonGameModule.cpp`.

### 3.4 Register the new sources in `core/CMakeLists.txt`

There is no glob. Add the new `.cpp` files (set source, card preview source, game module, and the card domain `.cpp` if you added one) to the `add_library(ccm_core ...)` argument list. Configure the build before moving on; this catches any missing headers immediately.

---

## 4. Core: tests for the new sources

Writing the tests now, before the UI work, makes the next sections noticeably faster — every later UI debugging session benefits from already knowing the parser and the URL builder are correct.

### 4.1 `<name>_set_source_tests.cpp`

Create `tests/<name>_set_source_tests.cpp` modeled on `tests/pokemon_set_source_tests.cpp`. The required cases are:

- `parseResponse` happy path with two or three sets, including the date rewrite if your API uses a non-`YYYY/MM/DD` format.
- `parseResponse` already-sorted output (input out of order, output ascending by release date).
- `parseResponse` empty array → empty `Result::ok(...)`.
- `parseResponse` missing top-level container → `Result::err(...)`.
- `parseResponse` malformed JSON → `Result::err(...)`.
- `fetchAll` happy path through a `FixedHttpClient` fake (in-file, ~10 lines — see the existing tests). Assert the `lastUrl` equals `kEndpoint`.
- `fetchAll` HTTP error → `Result::err(...)` propagation.

### 4.2 `<name>_card_preview_source_tests.cpp` (if you have a preview source)

Create `tests/<name>_card_preview_source_tests.cpp` modeled on `tests/pokemon_card_preview_source_tests.cpp`. The required cases are:

- `buildSearchUrl` percent-encodes names with spaces and reserved characters.
- `buildSearchUrl` includes / omits the `setId` clause based on whether `setId` is empty.
- `buildSearchUrl` includes / omits / normalizes `setNo` according to your normalization rules.
- `parseResponse` returns the preferred image variant.
- `parseResponse` falls back to the secondary variant when the primary is absent.
- `parseResponse` errors on empty `data[]`, missing `images`, malformed JSON.
- `fetchImageUrl` round-trips through `FixedHttpClient` and asserts the URL was percent-encoded as expected.
- `fetchImageUrl` propagates HTTP errors.

### 4.3 Register the new test files

Add `<name>_set_source_tests.cpp` (and `<name>_card_preview_source_tests.cpp` if applicable) to `tests/CMakeLists.txt` `add_executable(ccm_core_tests ...)`. Build and run `ctest --test-dir build --output-on-failure`. **Do not** continue until these pass.

### 4.4 Sorter / filter tests

If you introduced a new card type in §2.2, add cases to `tests/card_sorter_tests.cpp` and `tests/card_filter_tests.cpp` covering the new sort columns and filter columns introduced by your domain type. The boolean-flag exclusion rule (filter only checks string/number columns; flag columns are skipped) must be covered explicitly so future refactors don't quietly break it.

### 4.5 Set-service routing test

Append a case to `tests/set_service_tests.cpp` that registers your new module alongside Magic and verifies that `updateSets(Game::<Name>)` does not perturb cached data for the other game. Routing isolation is what `SetService` exists for; one test per game keeps it honest.

---

## 5. UI: derive from the three base templates

The UI layer is built on three header-only class templates that own all the wxWidgets-specific machinery. Each has a small, well-documented set of virtual hooks; deriving for a new game is a hook-implementation exercise, not a wxWidgets exercise. Read `ui_wx/include/ccm/ui/Base*.hpp` once before starting.

### 5.1 SVG icons

If your game introduces flag columns whose glyphs do not already exist in `ui_wx/include/ccm/ui/SvgIcons.hpp`, add them now:

- Declare each new icon as `extern const char* const kSvg<Name>;` in the header.
- Define them in `ui_wx/src/SvgIcons.cpp`. Keep the `@FILL@` placeholder so the rasterizer can substitute the active palette text color at draw time. **Do not** bake a color into the SVG — that breaks dark mode.
- Re-use existing glyphs (`kSvgSigned`, `kSvgAltered`, `kSvgFoil`, `kSvgHolo`) when the meaning matches.

### 5.2 Sort and filter helpers

Add a `<Name>SortColumn` enum to `core/include/ccm/services/CardSorter.hpp`, plus the corresponding `sort<Name>Cards(std::vector<<Name>Card>&, <Name>SortColumn, bool)` declaration. Implement it in `core/src/services/CardSorter.cpp` mirroring the existing per-column dispatch (each enum entry maps to a comparator).

Add `[[nodiscard]] bool matches<Name>Filter(const <Name>Card&, std::string_view)` to `core/include/ccm/services/CardFilter.hpp` and implement it in `core/src/services/CardFilter.cpp`. Walk the same value-key columns that the list panel will display, lowercase both sides, and short-circuit on any substring hit. Boolean flag columns are intentionally excluded — only string/number columns participate in filtering.

These functions are also the targets of §4.4's tests; you'll have already written the tests if you followed the order.

### 5.3 `<Name>CardListPanel`

Create:

- `ui_wx/include/ccm/ui/<Name>CardListPanel.hpp`
- `ui_wx/src/<Name>CardListPanel.cpp`

The header declares a `final class` deriving from `BaseCardListPanel<<Name>Card, <Name>SortColumn>`, with overrides for:

- `declareTextColumns()` — return a `std::vector<TextColumnSpec>` of `{label, width, format, optional<sortColumn>}`. The order is left-to-right on screen. The **last** entry must be the `Note` column; the base reserves it.
- `declareIconColumns()` — return a `std::vector<IconColumnSpec>` of `{svg, width, optional<sortColumn>}`. These render between the leading text columns and the Note column.
- `renderTextCell(card, idx)` — return the cell text for the `idx`-th text column. The base passes index `0..textCols-1` for the leading rows and `textCols-1` for the Note row, so you usually `switch (idx)`.
- `isIconColumnSet(card, idx)` — return whether the `idx`-th icon column should render its glyph for this row. Index `0` is the first icon column declared in `declareIconColumns()`.
- `sortBy(column, ascending)` — call `sort<Name>Cards(mutableCards(), column, ascending)`. **Use `mutableCards()`**, not `cards()`, because `sortBy` writes through the underlying vector.
- `matchesFilter(card, filter)` — call `matches<Name>Filter(card, filter)`.

In the constructor body, call `buildLayout()` (from `BaseCardListPanel`) so the base wires up the `wxListCtrl`, the themed header row, and the image lists.

The reference implementation at `ui_wx/src/PokemonCardListPanel.cpp` is ~75 lines including the icon-column declaration and the `switch`-based renderer. Yours should land in the same ballpark.

### 5.4 `<Name>SelectedCardPanel`

Create:

- `ui_wx/include/ccm/ui/<Name>SelectedCardPanel.hpp`
- `ui_wx/src/<Name>SelectedCardPanel.cpp`

Inside the `.cpp`, define an unnamed-namespace `enum <Name>DetailKey : int { ... };` with one entry per detail row and one per flag column. Keep these names local — they're only used between this file's hook overrides.

Override:

- `declareDetailRows()` — return a `std::vector<DetailRowSpec>` of `{label, key, emptyLabel}`. The **first** row should typically be `Name`; its `emptyLabel` is what the panel shows when no card is selected.
- `declareFlagIcons()` — return a `std::vector<FlagIconSpec>` of `{svg, tooltip, key}`.
- `detailValueFor(card, key)` — `switch (key)` and return the appropriate string. **Also handle `kNoteKey`** (defined in `BaseSelectedCardPanel` as `-1`); the base calls `detailValueFor(card, kNoteKey)` to populate the bottom Note row.
- `isFlagSet(card, key)` — `switch (key)` and return the matching boolean.
- `previewKey(card)` — return `std::tuple<std::string, std::string, std::string>` of `(name, setId, setNo)`. Use empty `setNo` for games whose preview API does not need a collector number.
- `gameId()` — return `Game::<Name>`.

In the constructor body, call `buildLayout()` so the base wires up the preview area, detail grid, flag strip, and image list.

The reference implementation is `ui_wx/src/PokemonSelectedCardPanel.cpp`.

### 5.5 `<Name>CardEditDialog`

Create:

- `ui_wx/include/ccm/ui/<Name>CardEditDialog.hpp`
- `ui_wx/src/<Name>CardEditDialog.cpp`

Derive from `BaseCardEditDialog<<Name>Card>`. Override:

- `buildFlagsRow(wxBoxSizer* flagsBox)` — create your `wxCheckBox`es and `flagsBox->Add(...)` them. The base owns the surrounding `Flags` label and sizer.
- `appendExtraRows(wxFlexGridSizer* grid)` — only if your game has fields beyond the standard set. Use the inherited `appendRow(grid, label, ctrl)` helper. (Pokemon adds a `Set #` text input here.)
- `readExtraFromCard()` — copy fields from `constCard()` into your widgets.
- `writeExtraToCard()` — copy values from your widgets back into `mutableCard()`.
- `updateMenuName()` — return `"Update <Display>"`. This is what the dialog's "no sets cached" hint shows the user.

In the constructor:

1. Pass through to the `BaseCardEditDialog` constructor with the dialog title (e.g. `"Add <Display> Card"` or `"Edit <Display> Card"` based on `EditMode`), `imageService`, `setService`, `mode`, `std::move(initial)`, `Game::<Name>`, and the optional `preloadedSets` pointer.
2. Call `buildAndPopulate()` (from the base) to build the form, populate the choices, and call `readExtraFromCard()`.

The reference implementation is `ui_wx/src/PokemonCardEditDialog.cpp`.

### 5.6 `<Name>GameView`

This is the polymorphic glue between the new game's panels and the rest of the app. Create:

- `ui_wx/include/ccm/ui/<Name>GameView.hpp`
- `ui_wx/src/<Name>GameView.cpp`

Derive from `IGameView`. The constructor takes references to the shared services (`ConfigService`, `SetService`, `ImageService`, `CardPreviewService`), the typed `CollectionService<<Name>Card>&`, and the `IGameModule&`. Members:

- `<Name>CardListPanel*     listPanel_{nullptr};`
- `<Name>SelectedCardPanel* selectedPanel_{nullptr};`
- `std::vector<Set>         setsCache_;` — populated lazily by `setsForDialog()` so each Add/Edit open does not re-read from `SetService`.

Implement the virtuals:

- `gameId()` returns `Game::<Name>`.
- `displayName()` returns `"<Display>"`.
- `listPanel(parent)` — lazily allocates the list panel as a child of `parent`; on first allocation, also `Bind(EVT_CARD_SELECTED, ...)` to push `listPanel_->selected()` into `selectedPanel_`. **The binding must live here**, in the typed `IGameView`, not in `MainFrame` — `MainFrame` only sees `IGameView` and never `<Name>Card`.
- `selectedPanel(parent)` — lazily allocates the selected panel.
- `refreshCollection()` — calls `collection_.list(Game::<Name>)`, handles errors with `wxMessageBox`, and pushes the new vector into `listPanel_->setCards(...)`. Also re-syncs the selected panel.
- `onAddCard(parent)`, `onEditCard(parent)`, `onDeleteCard(parent)` — open the typed `<Name>CardEditDialog` (or pop a confirm dialog for delete), call the typed `CollectionService` to commit, and refresh on success.
- `onUpdateSets(parent)` — calls `sets_.updateSets(Game::<Name>)`, refreshes `setsCache_`, returns a status string.
- `setFilter(filter)` — forwards to `listPanel_->setFilter(filter)`.
- `applyTheme(palette)` — forwards to both panels' `applyTheme`.
- `updateSetsMenuLabel()` — returns `"Update <Display>"`. This is what the `Sets` menu entry shows.

The reference implementation is `ui_wx/src/PokemonGameView.cpp`. It's about 160 lines and is the same shape for every game.

### 5.7 Register the new UI sources

Add **all** new UI `.cpp` files to `ui_wx/CMakeLists.txt`:

- `<Name>CardListPanel.cpp`
- `<Name>SelectedCardPanel.cpp`
- `<Name>CardEditDialog.cpp`
- `<Name>GameView.cpp`

There is no glob.

---

## 6. Composition root

Edit `app/main.cpp` to wire the new game in. Read `app/AGENTS.md` first — destruction-order rules apply.

### 6.1 New members

Add `std::unique_ptr<...>` members to `CcmApp`. Order matters (destruction is reverse — deps before dependents):

```cpp
std::unique_ptr<ccm::<Name>GameModule>                            <name>Mod_;
std::unique_ptr<ccm::JsonCollectionRepository<ccm::<Name>Card>>   <name>Repo_;
std::unique_ptr<ccm::CollectionService<ccm::<Name>Card>>          <name>CollSvc_;
std::unique_ptr<ccm::ui::<Name>GameView>                          <name>View_;
```

Place them next to the existing Magic/Pokemon members in the matching position — game module after `http_`, repo after the module, collection service after the repo and the image store, view at the end before `ctx_`.

### 6.2 New constructions in `OnInit()`

```cpp
<name>Mod_  = std::make_unique<ccm::<Name>GameModule>(*http_);

<name>Repo_ = std::make_unique<ccm::JsonCollectionRepository<ccm::<Name>Card>>(
    *fs_, *config_, &dirNameForGame);

<name>CollSvc_ = std::make_unique<ccm::CollectionService<ccm::<Name>Card>>(
    *<name>Repo_, *imgStore_);

setSvc_->registerModule(<name>Mod_.get());
previewSvc_->registerModule(*<name>Mod_);  // no-op when the module has no preview source

<name>View_ = std::make_unique<ccm::ui::<Name>GameView>(
    *config_, *<name>CollSvc_, *setSvc_, *imgSvc_, *previewSvc_, *<name>Mod_);
```

### 6.3 `dirNameForGame`

Add a `case ccm::Game::<Name>: return "<name>";` arm. The string must match `<Name>GameModule::dirName()`.

### 6.4 `AppContext`

`AppContext` (`ui_wx/include/ccm/ui/AppContext.hpp`) currently holds explicit references to `magicModule` and `pokemonModule`. Add an `<Name>Module` reference field — keep the alphabetical / canonical order — and pass `*<name>Mod_` for it in the `AppContext{...}` brace-init in `OnInit()`. Also append `<name>View_.get()` to the `gameViews` vector.

The `Game` and `Sets` menus in `MainFrame` are built dynamically from `gameViews`, so once the new view is in the vector its menu entries (the `Game > <Display>` radio item and the `Sets > Update <Display>` action) appear automatically.

---

## 7. AGENTS.md and tests housekeeping

After all the above compiles and tests pass:

1. **`AGENTS.md`** (root) — the "After adding a new game module you must" required follow-up should already cover your work; read it and confirm. If you added a new domain type, the matching `tests/domain_json_tests.cpp` round-trip test is required (per `core/AGENTS.md`).
2. **`core/AGENTS.md`** — update only if your game required a new core seam shape (a new port, a new service, a new shared helper). Describing the new game itself is not required; the doc is meant to stay game-agnostic.
3. **`ui_wx/AGENTS.md`** — same: update only if you needed a new template hook or had to teach the `Base*` templates a new behaviour. Describing the new game's panel set is not required.
4. **`app/AGENTS.md`** — confirm the "Composition root is the only place" allowlist still mentions the concrete adapter types. If you added new ones (a new `<Name>GameView`, `<Name>GameModule`), append them.
5. **`tests/AGENTS.md`** — add the new test file names to the file map and the "Required follow-ups" list.
6. **This file** (`docs/adding-a-new-game.md`) — only edit when the procedure itself changes (new template hook, new service registration, new composition-root step). Do not insert your specific game's quirks here; capture those in code comments next to the relevant overrides.

---

## 8. Verification checklist

Run, in order, from the workspace root. Do not skip any step.

1. **Configure**: `cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release` (or your usual generator). Configuration must succeed without warnings about a missing source file.
2. **Build**: `cmake --build build --parallel`. Must succeed cleanly. Pay close attention to template-instantiation errors — those usually indicate one of the `Base*` hooks is missing or wrongly-typed.
3. **Tests**: `ctest --test-dir build --output-on-failure`. Every existing test plus the new `<name>_set_source_tests`, `<name>_card_preview_source_tests`, the extended `domain_json_tests`, the extended `card_sorter_tests`, the extended `card_filter_tests`, and the extended `set_service_tests` must pass.
4. **Smoke test**: launch `./build/bin/ccm3` (or `.\build\bin\ccm3.exe`). Note: the CMake target is `ccm` but the executable is renamed to `ccm3` via `set_target_properties(... OUTPUT_NAME ccm3)`.
   - The `Game` menu shows your new game alongside Magic and Pokemon and switching is instantaneous (no panel re-creation cost on subsequent switches).
   - The `Sets > Update <Display>` action fetches sets and reports a count.
   - With sets cached, opening the new game's Add dialog populates the set picker and the dialog can be dismissed with `OK`.
   - Adding, editing, and deleting a card all round-trip through disk: close and re-open the app and the card persists.
   - Selecting a card kicks off a preview fetch (if the game has a preview source) and renders the image; the status line returns to `"Ready"` when the preview lands.
   - The flag-icon strip shows / hides per card depending on which flags are set.
   - Theme switching applies to all of the new game's panels (light → dark → light).

---

## 9. Common traps

These do not match a single seam in this guide but are worth calling out explicitly.

- **Stale set caches.** Each `IGameView` caches `std::vector<Set> setsCache_`. After `onUpdateSets` succeeds, refresh the cache (assign the new vector). The reference implementations do this.
- **`signed_` / `signed`.** The C++ field is `signed_`; the JSON key is `"signed"`. This is intentional and must not be changed. The same convention applies to any new field where the natural name collides with a C++ keyword — pick a trailing-underscore C++ name and an unaliased JSON key.
- **Spacer column index.** `BaseCardListPanel` reserves index `0` for a hidden zero-width spacer column (MSW comctl32 image-list gutter workaround). Real columns start at index `1`. If you ever need to call into `wxListCtrl` directly from a derived panel (you should not), remember this.
- **Preview-fetch threading.** The async preview fetch in `BaseSelectedCardPanel` uses a `shared_ptr<State>` + `std::atomic alive` + `std::atomic currentGen` triple. Do not capture `this` raw in any background work you add to a new game's selected panel; copy that pattern verbatim.
- **First-paint perf.** `MainFrame` defers initial collection load with `CallAfter(...)` and `BaseCardListPanel` defers the initial selection the same way. Don't move that work back into the constructor for "convenience" — it makes startup visibly slower.
- **`previewKey` for games without `setNo`.** If your preview API only needs `(name, setId)`, return an empty string for the third tuple element. The base will pass `""` through to the source, which is exactly what `MagicCardPreviewSource` is built to handle.
- **Filter exclusion.** The filter intentionally ignores boolean-flag columns. If you find yourself wanting `signed:true` style filters, that is a future feature, not a fix; don't smuggle it into `matches<Name>Filter` without a design discussion.
- **Theming dialogs.** Always `applyThemeToWindowTree(&dlg, palette, theme)` before `ShowModal()` for any dialog you open. The reference `<Name>GameView::onAddCard` / `onEditCard` show the canonical pattern.

---

## 10. Where to read first when something does not work

- The new game compiles but its menu entries do not appear — check that the view was appended to `AppContext::gameViews` in `app/main.cpp`.
- The list panel is empty even after `Sets > Update <Display>` succeeds — check `dirNameForGame`. The repository writes to `<dataStorage>/<dirName>/collection.json`, and a typo here makes the load silently return an empty list on next launch.
- The filter input does nothing on the new game — check that `<Name>GameView::setFilter(...)` forwards to the list panel and that `matches<Name>Filter` actually evaluates the active filter substring (the empty filter must match every row).
- The Add dialog shows `(no sets cached - use Sets > Update <Display>)` even after a successful update — `setsCache_` was not refreshed in `onUpdateSets`. The reference views assign `out.value()` into the cache.
- Preview never resolves — first add a unit test that hits `parseResponse` with a real captured response body. If that passes, log the URL `IHttpClient::get` is called with and try it in a browser or `curl`. Most "broken preview" bugs are URL encoding or a wrong shape in `buildSearchUrl`.
- Sort works but its arrow indicator is wrong — column `0` is the spacer, so the visual column index sort key cares about is one higher than you might expect. The base handles this; if it goes wrong, check that your `TextColumnSpec`/`IconColumnSpec` order matches `renderTextCell` / `isIconColumnSet` indexing exactly.
- Tests pass but the app crashes on shutdown — destruction order in `CcmApp` is wrong. Move `<name>View_` so it is declared **after** `<name>CollSvc_`, `setSvc_`, `imgSvc_`, `previewSvc_`, `<name>Mod_` — the view must be torn down before any of its referenced services.
