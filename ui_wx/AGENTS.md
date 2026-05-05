# ui_wx/AGENTS.md

`ccm_ui_wx` static library — wxWidgets adapter. The **only** target that may include `wx/...` headers. Read the root `AGENTS.md` first.

## Layer pointers

- `include/ccm/ui/AppContext.hpp` — the boundary type. A struct of references to core services + the active `IGameModule`s. UI code must talk to core only through this struct.
- `include/ccm/ui/MainFrame.hpp` + `src/MainFrame.cpp` — top-level window and frame-level event wiring. On Windows dark mode, the native menubar/statusbar are not reliably themeable; use the app-owned themed menu strip (`File` / `Game` / `Sets`) and bottom status panel text instead of relying on OS menubar/statusbar colors. First interactive row under menu strip is a horizontal toolbar (`wxBitmapButton` glyphs for Add / Edit / Delete, tooltips `"Add Card"` / `"Edit"` / `"Delete"`), left-aligned with a stretch spacer and filter `wxTextCtrl` (`"Filter"`) on the right. Below that, a vertical splitter fills the remainder (`proportion 1`) — preview panel left, `MagicCardListPanel` right. On `wxEVT_TEXT` the frame pushes filter text into `MagicCardListPanel::setFilter(...)`. Binds `EVT_MAGIC_CARD_SELECTED` (list selection -> details panel) and `EVT_PREVIEW_STATUS` (preview fetch outcome -> status label; empty string resets to `"Ready"`).
- `include/ccm/ui/MagicCardListPanel.hpp` + `src/MagicCardListPanel.cpp` — `wxListCtrl` view of the Magic collection. Column layout remains hidden spacer | Name | Set | Amount | Condition | Language | foil | signed | altered | Note. Native `wxListCtrl` header theming is unreliable in Windows dark mode, so a custom themed header row is used above the list (`wxLC_NO_HEADER` on the list). Preserve native-like behavior: single-click header sorts, edge-drag resizes column width, divider double-click autosizes to content. The Set column displays `set.name` but sorts by `set.releaseDate` (same `valueKey`/`sortKey` separation as the table model). Free-text filtering follows the table `applyFilter` semantics; input remains owned by `MainFrame`, while panel recomputes `filteredIndices_` and rebuilds visible rows. Selection mapping (`selected()`, `applyRowSelectionStyle`) goes through `cardForRow(row)`. **Rebuild guard**: `rebuildRows()` sets `inRebuild_=true` while rows churn so DESELECTED/SELECTED noise doesn't bubble; emit exactly one `EVT_MAGIC_CARD_SELECTED` via `notifySelectionChanged()` after settle.
- `include/ccm/ui/SelectedCardPanel.hpp` + `src/SelectedCardPanel.cpp` — preview image (fetched via `CardPreviewService` and Scryfall on selection) at the top, then a 2-column `wxFlexGridSizer` of `bold label | value` rows, plus a flag-icon row (foil/signed/altered) and a `wxListBox` of `Image N` entries that opens `ImageViewerDialog` on double-click. Resolves stored image paths through `ImageService::resolveImagePath`. `setCard()` only restarts the Scryfall fetch when the card id actually changed (tracked via `lastFetchedId_`); same-card re-renders update labels but reuse the in-flight / completed preview to avoid redundant HTTP calls. Emits `EVT_PREVIEW_STATUS` when the fetch resolves: empty string on success, `"Preview unavailable: <reason>"` on failure (with the underlying `Result` error string preserved verbatim — connectivity issues, HTTP 429 rate limits, and Scryfall semantic failures all surface here).
- `include/ccm/ui/SvgIcons.hpp` + `src/SvgIcons.cpp` — embedded SVG templates (`kSvgFoil` / `kSvgSigned` / `kSvgAltered`, based on react-icons artwork) plus toolbar glyphs (`kSvgToolbarAdd` / `kSvgToolbarEdit` / `kSvgToolbarDelete` from Microsoft's vscode-codicons). `@FILL@` placeholder lets callers pass a runtime color (typically `wxSYS_COLOUR_WINDOWTEXT` / `wxSYS_COLOUR_HIGHLIGHTTEXT`). `svgIconBitmap` / `paddedSvgIcon` helpers backed by `wxBitmapBundle::FromSVG`. Toolbar buttons use `svgIconBitmap` with `wxSYS_COLOUR_BTNTEXT`.
- `include/ccm/ui/CardEditDialog.hpp` + `src/CardEditDialog.cpp` — modal create/edit form for `MagicCard`. Reads sets from `SetService` and writes images via `ImageService`. Set picker is a read-only `wxComboBox` listing **set name only**. Edit-mode preselection must match `card_.set.id` against `sets_[].id` **case-insensitively** (legacy collections may store upper-case IDs while current set feeds are lower-case); only fall back to index 0 when no case-insensitive match exists. Incremental **type-to-find** is implemented explicitly via combo-level key handling (`setCombo_->Bind(wxEVT_CHAR, ...)`): case-insensitive prefix match on `sets_[].name`, 1000 ms idle resets the buffer, and backspace edits the active prefix. Keep this path authoritative so native single-letter jump behavior does not override multi-character matching. Image picker behavior must preserve the existing UX: use a single open action to import **multiple selected files** (`wxFD_MULTIPLE` + `GetPaths`). Existing image rows in the dialog are previewable: double-click opens `ImageViewerDialog` at the clicked index, using `ImageService::resolveImagePath` to build the full path list.
- `include/ccm/ui/SettingsDialog.hpp` + `src/SettingsDialog.cpp` — edits `Configuration` via `ConfigService::store`.
- `include/ccm/ui/ImageViewerDialog.hpp` + `src/ImageViewerDialog.cpp` — full-size viewer with prev/next.

## Conventions

1. **Only consume core through `AppContext`.** Do not include any header from `ccm/infra/` here. The set of allowed `ccm/...` includes is `domain/`, `services/`, `games/IGameModule.hpp`, and `util/Result.hpp`.
2. **Image decoding lives here, not in core.** Use `wxImage::LoadFile(path.string())` against the path returned by `IImageStore::resolvePath`. Core stays free of any image library.
3. **Ownership**: dialogs and panels are heap-allocated and parented to a `wxWindow`. wxWidgets owns the lifetime — do **not** wrap them in `unique_ptr`.
4. **Custom events** propagate from a child via `parent->GetEventHandler()->ProcessEvent(ev)`. Do not rely on `ProcessWindowEvent` to bubble across windows. See `MagicCardListPanel::onSelectionChanged` for the pattern.
5. **wxFont modifications** mutate in place: `font.MakeBold().MakeLarger()` — do not call `Scale` (it does not exist on wxFont 3.2; use `MakeLarger` / `SetPointSize`).
6. **Pokemon items stay disabled.** When you add new Pokemon UI, gate it on a `Game` switch and disable the menu/toolbar items if the active game is unsupported. See `MainFrame::buildMenuBar`.
7. **No `ccm_warnings`.** This target intentionally does **not** link the strict warning interface — wxWidgets headers trip `-Wpedantic` / `-Wshadow`. Keep it that way; do not add the link.
8. **Async background work** must not capture `this` raw. Use the pattern from `SelectedCardPanel`: a `std::shared_ptr<State>` holding `std::atomic<bool> alive`, `std::atomic<unsigned> currentGen`, and a back-pointer to the panel; spawn a detached `std::thread`, then deliver the result with `wxTheApp->CallAfter([state, gen, ...]() { if (!state->alive) return; if (state->currentGen != gen) return; ... })`. Flip `alive=false` in the panel destructor so late callbacks become no-ops.
9. **Icons come from `SvgIcons.hpp`.** Don't inline new SVG strings in panel sources; add them to `SvgIcons.{hpp,cpp}` so both panels stay in sync. Always pass a runtime fill color (`wxSystemSettings::GetColour(...).GetAsString(wxC2S_HTML_SYNTAX)`); never bake one into the SVG.
10. **Sort key != display key.** When you add a new column to a list panel, follow `MagicCardListPanel`'s split: the `wxListCtrl` cell text is one thing; the *sort* comparator lives in `ccm::services::CardSorter` and may key off a different field (the canonical case is `set.name` shown but `set.releaseDate` sorted, so collections list chronologically). New columns must extend `MagicSortColumn` / `PokemonSortColumn` and add a corresponding `case` in `sortMagicCards` / `sortPokemonCards`.
11. **`wxListCtrl` + image-list pitfalls (MSW comctl32):**
    - When a small image list is attached, the leftmost column always reserves an item-icon gutter even with `iImage = -1`. Hide it by **prepending a zero-width spacer column** (see `kColSpacer` in `MagicCardListPanel`) and start real columns at index 1.
    - Insert each row through that spacer column with a `wxListItem` whose mask includes `wxLIST_MASK_IMAGE` and image `-1`. The shorthand `InsertItem(long, wxString)` overload omits the image mask, and on MSW `LVS_REPORT` defaults the item's iImage to 0 — leaking the first image-list entry into column 0.
    - Create the image list with **`hasMask=false`** so it stays `ILC_COLOR32`-only. With a mask, `LVS_REPORT` auto-applies `ILD_SELECTED`'s 50% highlight blend on selected rows and the derived 1-bit mask draws the whole padded bitmap as an opaque tinted rectangle.
    - For "white icon on highlight when selected", store **two variants** of every glyph in the same image list (dark for headers + unselected rows, highlight-text for the selected row) and swap them in `wxEVT_LIST_ITEM_SELECTED` / `_DESELECTED` via `SetItem(li)` with `wxLIST_MASK_IMAGE`. There is no native sub-item selImage on MSW.
    - Header bitmaps are anchored at the column's left edge regardless of `wxLIST_FORMAT_CENTER`. To get a visually centered header glyph, use `paddedSvgIcon(svg, iconSize, wxSize(colContentWidth, iconSize), fillHex)` so the icon sits centered inside a wider transparent canvas.
12. **Startup/dialog responsiveness rules:**
    - Keep first paint fast: avoid heavy synchronous work in window/dialog constructors.
    - In `MainFrame`, defer initial collection load with `CallAfter(...)` so the frame paints before I/O/parsing.
    - Keep startup's "first row selected" behavior, but schedule initial selection with `CallAfter(...)` in `MagicCardListPanel` to avoid blocking first render.
    - Avoid reloading/reparsing sets on each Add/Edit open: reuse `MainFrame`'s cached set list.
    - Pass preloaded sets into `CardEditDialog` by pointer/reference (not by value) to avoid vector copies per open.
    - For heavy dialog setup, wrap constructor-time UI population in `Freeze()` / `Thaw()` and append choice items in bulk via `wxArrayString`.
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
- After adding a new menu action you **must** allocate an `Ids::*` value in `MainFrame.hpp` (don't reuse wxID_HIGHEST math inline) and `Bind` it in `buildMenuBar`.
- After changing `AppContext` you **must** update `app/main.cpp` so the composition root populates the new field.
- After adding a new icon to `SvgIcons.{hpp,cpp}` you **must** keep the `@FILL@` placeholder so both light- and dark-variant rendering keeps working, and add a small unit-test-equivalent visual check by running the binary (no automated UI tests in this repo).

## Adding Pokemon (or another game) UI

1. Add a `<Name>CardListPanel`, `<Name>SelectedCardPanel`, `<Name>EditDialog` under `include/ccm/ui/` and `src/` mirroring the Magic ones.
2. Extend `AppContext` with a `CollectionService<<Name>Card>&` reference. The composition root creates the service.
3. Re-enable the existing `IdSwitchPokemon` / `IdUpdateSetsPokemon` menu items in `MainFrame::MainFrame` (currently disabled via `GetMenuBar()->Enable(..., false)`).

## Commands

Build UI only: `cmake --build build --target ccm_ui_wx`
