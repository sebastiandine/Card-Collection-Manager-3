# ui_wx/AGENTS.md

`ccm_ui_wx` static library — wxWidgets adapter. The **only** target that may include `wx/...` headers. Read the root `AGENTS.md` first.

## Layer pointers

- `include/ccm/ui/AppContext.hpp` — the boundary type. A struct of references to shared core services + per-game modules and a `std::vector<IGameView*>` of all UI bundles. UI code talks to core only through this struct (and the typed pointers go through `IGameView`, never directly).
- `include/ccm/ui/IGameView.hpp` — abstract base class for per-game UI bundles. `MainFrame` only ever sees `IGameView` references; this is the seam that lets the frame swap between Magic, Pokemon, and any future TCG without knowing their card types.
- `include/ccm/ui/MainFrame.hpp` + `src/MainFrame.cpp` — top-level window, menu strip (`File` / `Game` / `Sets`), toolbar (Add / Edit / Delete + filter input), and the splitter that swaps the active `IGameView`'s panels. The `Game` and `Sets` menus are built dynamically from `AppContext::gameViews` so adding a new game lights up its menu entries automatically. Filter and toolbar actions forward to `activeView()`. `EVT_PREVIEW_STATUS` (preview fetch outcome → status label; empty string resets to `"Ready"`) is the only event the frame binds; `EVT_CARD_SELECTED` is bound *per view* (each `IGameView` connects its typed list panel to its typed selected panel internally).
- `include/ccm/ui/BaseCardListPanel.hpp` — header-only template `BaseCardListPanel<TCard, TSortColumn>` that owns ALL the non-game-specific `wxListCtrl` machinery: hidden zero-width spacer column (MSW comctl32 image-list gutter workaround), themed header row (clickable to sort, edge-drag to resize, divider double-click to autosize), two-variant icon image list (dark glyph for unselected rows / headers, light glyph for the selected row), rebuild guard so DESELECTED/SELECTED storms collapse into a single bubbled `EVT_CARD_SELECTED`, case-insensitive substring filter via `setFilter(...)`, per-column toggle-direction sort. Subclasses fill in column descriptors + per-row text + per-icon-column flag predicates + dispatch hooks (`sortBy`, `matchesFilter`).
- `include/ccm/ui/BaseSelectedCardPanel.hpp` — header-only template `BaseSelectedCardPanel<TCard>` that owns the right-hand-side detail panel: preview image fetched via `CardPreviewService` (with the `shared_ptr<State>` + `std::atomic alive`/`currentGen` cancellation pattern), 2-column detail grid, flag-icon strip that collapses when no flags are set, image list with double-click viewer. Subclasses describe the detail rows / flag icons / preview lookup `(name, setId, setNo)` and own a `Game` constant.
- `include/ccm/ui/BaseCardEditDialog.hpp` — header-only template `BaseCardEditDialog<TCard>` that owns the standard Add/Edit form: Name, Set picker (read-only `wxComboBox` with prefix-match typeahead and case-insensitive id matching for legacy data), Amount spin, Language and Condition choices, Note, image management (Add multiple via `wxFD_MULTIPLE`, Remove, double-click to view), OK/Cancel + validation. Subclasses build the flags row (`buildFlagsRow`), append game-specific extra rows (e.g. Pokemon's `Set #`) via `appendExtraRows`, and copy values in/out of the typed card (`readExtraFromCard` / `writeExtraToCard`).
- `include/ccm/ui/Magic*.hpp` + `src/Magic*.cpp` — Magic implementations: `MagicCardListPanel`, `MagicSelectedCardPanel`, `MagicCardEditDialog`, `MagicGameView`. Each is ~50–100 lines of hook overrides on top of the matching base template.
- `include/ccm/ui/Pokemon*.hpp` + `src/Pokemon*.cpp` — Pokemon implementations: `PokemonCardListPanel`, `PokemonSelectedCardPanel`, `PokemonCardEditDialog`, `PokemonGameView`. Same shape as the Magic ones; differences are limited to the Set # field, the Holo / 1. Edition flags, and the Pokemon TCG preview lookup key (which includes `setNo`).
- `include/ccm/ui/SvgIcons.hpp` + `src/SvgIcons.cpp` — embedded SVG templates with a `@FILL@` placeholder. Magic flags: `kSvgFoil` / `kSvgSigned` / `kSvgAltered`. Pokemon flags: `kSvgHolo` (sparkle, mirroring the original `IconHolo` from `PokemonTable.tsx`) and `kSvgFirstEdition` (a "1." badge rebuilt from the original `IconPokemonFirstEdition.tsx`). Toolbar glyphs: `kSvgToolbarAdd` / `kSvgToolbarEdit` / `kSvgToolbarDelete` (vscode-codicons). `svgIconBitmap` / `paddedSvgIcon` helpers backed by `wxBitmapBundle::FromSVG`.
- `src/BaseEvents.cpp` — single-translation-unit definitions for `EVT_CARD_SELECTED` and `EVT_PREVIEW_STATUS`. Both events are template-instantiation-agnostic so all per-game panels share the same event types.
- `include/ccm/ui/SettingsDialog.hpp` + `src/SettingsDialog.cpp` — edits `Configuration` via `ConfigService::store`.
- `include/ccm/ui/ImageViewerDialog.hpp` + `src/ImageViewerDialog.cpp` — full-size viewer with prev/next.

## Conventions

1. **Only consume core through `AppContext`.** Do not include any header from `ccm/infra/` here. The set of allowed `ccm/...` includes is `domain/`, `services/`, `games/IGameModule.hpp`, `ports/ICardPreviewSource.hpp`, and `util/Result.hpp`.
2. **Image decoding lives here, not in core.** Use `wxImage::LoadFile(path.string())` against the path returned by `IImageStore::resolvePath`. Core stays free of any image library.
3. **Ownership**: dialogs and panels are heap-allocated and parented to a `wxWindow`. wxWidgets owns the lifetime — do **not** wrap them in `unique_ptr`. `IGameView` instances themselves are owned by `app/main.cpp` (`std::unique_ptr<>`); the panels owned by the views become children of the `MainFrame` splitter on first mount.
4. **Custom events**: `EVT_CARD_SELECTED` is fired by the list panel on itself (not its parent). Each `IGameView` binds it on its typed list panel inside the panel's first construction so the typed selection flows directly into the typed selected panel — `MainFrame` never sees a `MagicCard` or a `PokemonCard`. Do not move that binding back into `MainFrame`.
5. **wxFont modifications** mutate in place: `font.MakeBold().MakeLarger()` — do not call `Scale` (it does not exist on wxFont 3.2; use `MakeLarger` / `SetPointSize`).
6. **Single-active-game UX.** `MainFrame` only ever shows one game's panels at a time; the splitter swaps `listPanel()` / `selectedPanel()` when the user picks a different `Game` menu entry. Do not stand up parallel side-by-side tabs for different games.
7. **No `ccm_warnings`.** This target intentionally does **not** link the strict warning interface — wxWidgets headers trip `-Wpedantic` / `-Wshadow`. Keep it that way; do not add the link.
8. **Async background work** must not capture `this` raw. Use the pattern from `BaseSelectedCardPanel`: a `std::shared_ptr<State>` holding `std::atomic<bool> alive`, `std::atomic<unsigned> currentGen`, and a back-pointer to the panel; spawn a detached `std::thread`, then deliver the result with `wxTheApp->CallAfter([state, gen, ...]() { if (!state->alive) return; if (state->currentGen != gen) return; ... })`. Flip `alive=false` in the panel destructor so late callbacks become no-ops.
9. **Icons come from `SvgIcons.hpp`.** Don't inline new SVG strings in panel sources; add them to `SvgIcons.{hpp,cpp}` so all panels stay in sync. Always pass a runtime fill color (`wxSystemSettings::GetColour(...).GetAsString(wxC2S_HTML_SYNTAX)`); never bake one into the SVG.
10. **Sort key != display key.** When you add a new column to a list panel, follow the existing pattern: the `wxListCtrl` cell text is one thing; the *sort* comparator lives in `ccm::services::CardSorter` and may key off a different field (the canonical case is `set.name` shown but `set.releaseDate` sorted, so collections list chronologically). New columns must extend `MagicSortColumn` / `PokemonSortColumn` and add a corresponding `case` in `sortMagicCards` / `sortPokemonCards`.
11. **`wxListCtrl` + image-list pitfalls (MSW comctl32):**
    - When a small image list is attached, the leftmost column always reserves an item-icon gutter even with `iImage = -1`. Hide it by **prepending a zero-width spacer column** (already handled in `BaseCardListPanel::buildLayout`). Real columns start at index 1.
    - Insert each row through that spacer column with a `wxListItem` whose mask includes `wxLIST_MASK_IMAGE` and image `-1`. The shorthand `InsertItem(long, wxString)` overload omits the image mask, and on MSW `LVS_REPORT` defaults the item's iImage to 0 — leaking the first image-list entry into column 0.
    - Create the image list with **`hasMask=false`** so it stays `ILC_COLOR32`-only. With a mask, `LVS_REPORT` auto-applies `ILD_SELECTED`'s 50% highlight blend on selected rows and the derived 1-bit mask draws the whole padded bitmap as an opaque tinted rectangle.
    - For "white icon on highlight when selected", store **two variants** of every glyph in the same image list (dark for headers + unselected rows, highlight-text for the selected row) and swap them in `wxEVT_LIST_ITEM_SELECTED` / `_DESELECTED` via `SetItem(li)` with `wxLIST_MASK_IMAGE`. There is no native sub-item selImage on MSW.
    - Header bitmaps are anchored at the column's left edge regardless of `wxLIST_FORMAT_CENTER`. To get a visually centered header glyph, use `paddedSvgIcon(svg, iconSize, wxSize(colContentWidth, iconSize), fillHex)` so the icon sits centered inside a wider transparent canvas.
12. **Startup/dialog responsiveness rules:**
    - Keep first paint fast: avoid heavy synchronous work in window/dialog constructors.
    - In `MainFrame`, defer initial collection load with `CallAfter(...)` so the frame paints before I/O/parsing.
    - Keep startup's "first row selected" behavior, but schedule initial selection with `CallAfter(...)` in `BaseCardListPanel` to avoid blocking first render.
    - Avoid reloading/reparsing sets on each Add/Edit open: each `IGameView` caches its own set list and passes it into the dialog by pointer.
    - Pass preloaded sets into `BaseCardEditDialog` by pointer/reference (not by value) to avoid vector copies per open.
    - For heavy dialog setup, wrap constructor-time UI population in `Freeze()` / `Thaw()` and append choice items in bulk via `wxArrayString` (`BaseCardEditDialog::buildAndPopulate` does this).
13. **Theme consistency rules (Windows):**
    - Treat dialog roots as `panelBg`, not a separate shade, otherwise label rows can look like mismatched darker boxes.
    - Theme dialogs before `ShowModal()` with `applyThemeToWindowTree(...)`; this includes Settings and Create/Edit dialogs.
    - Include `wxSpinCtrl` in themed input controls (Amount field) or it will keep a mismatched native background.
    - Do not call `applyNativeClassTheme(..., "DarkMode_Explorer", "Explorer")` for `wxTextCtrl`; on some Windows builds this causes black typed text in dark mode. Keep text inputs palette-driven (`SetThemeEnabled(false)` in dark/high-contrast as needed).
    - If a specific text field still renders wrong while typing (notably `MainFrame`'s filter box), enforce text/background in `MainFrame::MSWWindowProc` via `WM_CTLCOLOREDIT` for that control handle.
    - Keep toolbar button behavior stable under dark/high-contrast: avoid changes that break click/tooltip affordances while experimenting with hover contrast fixes.
    - For dark/high-contrast button readability, do not trust native hover/pressed rendering on Windows; custom state painting in `Theme.cpp` is allowed when native visuals ignore configured colors.
    - Button event handlers must use per-button state that is refreshed when theme changes. Avoid one-time captures of theme colors/mode in lambdas; these can leak dark-mode behavior into light mode.
    - In High Contrast, use stronger hover/pressed deltas than regular dark mode and keep the button border in the foreground/text color for visibility (currently yellow in this palette).
    - When validating UI theming changes, rebuild and run `ccm` (the executable), not just `ccm_ui_wx`.

## Required follow-ups

- After adding a new dialog/panel `.cpp` you **must** add it to `ui_wx/CMakeLists.txt`.
- After adding a new menu action you **must** allocate an `Ids::*` value in `MainFrame.hpp` (don't reuse `wxID_HIGHEST` math inline) and `Bind` it in `buildMenuBar`. The dynamic Game / Sets menus consume the `IdGameMenuBase` / `IdSetsMenuBase` ranges; do not stomp on those id ranges.
- After changing `AppContext` you **must** update `app/main.cpp` so the composition root populates the new field.
- After adding a new icon to `SvgIcons.{hpp,cpp}` you **must** keep the `@FILL@` placeholder so both light- and dark-variant rendering keeps working, and add a small unit-test-equivalent visual check by running the binary (no automated UI tests in this repo).
- After changing one of the `Base*` template hooks (or adding a new one) you **must** keep `docs/adding-a-new-game.md` in sync — the per-game derived classes are the readers of that contract and the doc is what onboarding agents read first.

## Adding a new game UI

1. Implement three derived classes under `include/ccm/ui/` mirroring the Magic / Pokemon trio:
    - `<Name>CardListPanel : public BaseCardListPanel<<Name>Card, <Name>SortColumn>` — override `declareTextColumns()`, `declareIconColumns()`, `renderTextCell()`, `isIconColumnSet()`, `sortBy()`, `matchesFilter()`.
    - `<Name>SelectedCardPanel : public BaseSelectedCardPanel<<Name>Card>` — override `declareDetailRows()`, `declareFlagIcons()`, `detailValueFor()`, `isFlagSet()`, `previewKey()`, `gameId()`. Define a local `enum` of `DetailKey` constants for clarity.
    - `<Name>CardEditDialog : public BaseCardEditDialog<<Name>Card>` — override `buildFlagsRow()`, optionally `appendExtraRows()`, `readExtraFromCard()`, `writeExtraToCard()`, `updateMenuName()`.
2. Add a `<Name>GameView : public IGameView` that owns those panels and the typed `CollectionService<<Name>Card>&`. Bind `EVT_CARD_SELECTED` on the list panel inside `listPanel(parent)` to push the typed selection into the selected panel. The `MagicGameView` / `PokemonGameView` pair is the canonical reference.
3. Re-add the new view to `AppContext::gameViews` in the composition root (`app/main.cpp`). The `Game` and `Sets` menus pick it up automatically.
4. Add SVG glyphs for any new flag columns to `SvgIcons.{hpp,cpp}` (with the `@FILL@` placeholder).
5. Register all new `.cpp` files in `ui_wx/CMakeLists.txt`.

## Commands

Build UI only: `cmake --build build --target ccm_ui_wx`
