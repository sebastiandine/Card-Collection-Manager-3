#pragma once

// BaseCardListPanel<TCard, TSortColumn>
//
// Header-only template that owns ALL the non-game-specific machinery for the
// `wxListCtrl`-backed card table:
//
//   - hidden zero-width spacer column (MSW comctl32 image-list gutter
//     workaround; see `ui_wx/AGENTS.md` for the rationale)
//   - app-owned themed header row (clickable to sort, edge-drag to resize,
//     divider double-click to autosize) - native `wxListCtrl` header is
//     unreliable in Windows dark mode
//   - two-variant icon image list (dark glyph for unselected rows + headers,
//     selection-text-colored glyph for the selected row)
//   - rebuild guard so DESELECTED/SELECTED storms during rebuild collapse
//     into a single bubbled `EVT_CARD_SELECTED` event
//   - case-insensitive substring filter via `setFilter(...)` and per-column
//     toggle-direction sort via the header click
//
// Game-specific behavior is exposed as virtual hooks the derived class fills
// in (template method pattern):
//
//   declareTextColumns()   -> spec list (label, width, format) for the leading
//                             "value-key" columns and the trailing Note column
//   declareIconColumns()   -> spec list (svg, width, sortColumn) for icon-only
//                             flag columns (foil/signed/altered, holo, ...)
//   renderTextCell(card, idx) -> cell string for text column `idx`
//   isIconColumnSet(card, idx) -> whether the n-th icon column shows for this card
//   sortColumnForListIdx(col)  -> map physical wxListCtrl column to sort key
//   sortBy(col, asc)           -> in-place stable sort of `cards_`
//   matchesFilter(card, f)     -> case-insensitive row matcher
//
// New games extend this template — see `MagicCardListPanel` and
// `PokemonCardListPanel` for the canonical patterns.

#include "ccm/ui/SvgIcons.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/colour.h>
#include <wx/cursor.h>
#include <wx/event.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/utils.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ccm::ui {

// Single shared selection-changed event. The base panel raises this on the
// parent every time the active card changes (after a rebuild settles, after a
// user click, etc.). Defined once in `BaseEvents.cpp` so the wxEvent table is
// not duplicated per template instantiation.
wxDECLARE_EVENT(EVT_CARD_SELECTED, wxCommandEvent);

template <typename TCard, typename TSortColumn>
class BaseCardListPanel : public wxPanel {
public:
    using card_type = TCard;
    using sort_column_type = TSortColumn;

    // Replace the displayed rows. Selection is reset (the panel will pick
    // the first row on the next idle turn — see rebuildRows()).
    void setCards(std::vector<TCard> cards) {
        cards_ = std::move(cards);
        // Drop sort state when the underlying data is replaced - the indicator
        // shown in the header should match the order actually rendered, and
        // wxListCtrl keeps the indicator across DeleteAllItems().
        nextDirByCol_.clear();
        list_->RemoveSortIndicator();
        rebuildRows();
        if (!autoSizedOnce_ && !cards_.empty()) {
            autoSizeAllColumns();
            autoSizedOnce_ = true;
        }
    }

    // Update the filter string and rebuild the visible rows in place. The
    // panel preserves the previously-selected card across the rebuild when
    // it still matches the new filter; otherwise the first remaining row is
    // selected, or none if the filter excluded everything. A single
    // EVT_CARD_SELECTED is emitted afterwards so the parent re-syncs.
    void setFilter(std::string_view filter) {
        if (filter_ == filter) return;
        filter_.assign(filter);
        std::optional<std::uint32_t> keepId;
        if (auto sel = selected()) keepId = sel->id;
        rebuildRows(keepId);
    }

    void applyTheme(const ThemePalette& palette) {
        list_->SetBackgroundColour(palette.inputBg);
        list_->SetForegroundColour(palette.inputText);
        SetBackgroundColour(palette.panelBg);
        SetForegroundColour(palette.text);
        rebuildIconList(palette.inputText, wxColour(255, 255, 255));
        refreshHeaderTheme(palette);
        std::optional<std::uint32_t> keepId;
        if (auto sel = selected()) keepId = sel->id;
        rebuildRows(keepId);
        Refresh();
    }

    [[nodiscard]] const std::vector<TCard>& cards() const noexcept { return cards_; }
    [[nodiscard]] const std::string&        filter() const noexcept { return filter_; }
    [[nodiscard]] std::optional<TCard>      selected() const {
        const long sel = list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (const TCard* c = cardForRow(sel)) return *c;
        return std::nullopt;
    }

protected:
    // Column descriptor types -------------------------------------------------

    struct TextColumnSpec {
        std::string        label;
        int                width;
        wxListColumnFormat format;  // wxLIST_FORMAT_LEFT / RIGHT / CENTER
        std::optional<TSortColumn> sortColumn;  // none = not sortable
    };

    struct IconColumnSpec {
        const char* svg;
        int         width;
        std::optional<TSortColumn> sortColumn;
    };

    // Subclass hooks ----------------------------------------------------------

    // Subclass declares its leading text columns (Name, Set, ...). Order
    // matches the on-screen left-to-right ordering. The trailing "Note" column
    // is also returned here as the last entry — it is added AFTER the icon
    // columns by the base.
    [[nodiscard]] virtual std::vector<TextColumnSpec> declareTextColumns() const = 0;

    // Subclass declares the icon flag columns (Foil/Signed/Altered, etc.).
    // These render between the leading text columns and the trailing Note.
    [[nodiscard]] virtual std::vector<IconColumnSpec> declareIconColumns() const = 0;

    [[nodiscard]] virtual std::string renderTextCell(const TCard& card, std::size_t idx) const = 0;
    [[nodiscard]] virtual bool        isIconColumnSet(const TCard& card, std::size_t idx) const = 0;

    virtual void sortBy(TSortColumn column, bool ascending) = 0;
    [[nodiscard]] virtual bool matchesFilter(const TCard& card, std::string_view filter) const = 0;

    // Construction ------------------------------------------------------------

    explicit BaseCardListPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {}

    // Subclass calls this once from its constructor body (after virtual hooks
    // are reachable) to wire up columns + the header row + image list.
    void buildLayout() {
        list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                               wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);

        textCols_ = declareTextColumns();
        iconCols_ = declareIconColumns();

        // Note must be the *last* text column. We render it after the icons.
        // Layout: [hidden spacer] [textCols_-1 leading text cols] [icon cols] [last text col].
        if (textCols_.empty()) {
            // No text columns at all is unsupported; the trailing note column
            // is required by the panel layout.
            textCols_.push_back({"Note", 220, wxLIST_FORMAT_LEFT, std::nullopt});
        }

        buildHeaderRow();

        // Column 0 is a hidden spacer that swallows MSW's mandatory item-icon
        // gutter. All real columns live at index >= 1.
        list_->AppendColumn("", wxLIST_FORMAT_LEFT, 0);

        // Leading text columns (everything except the last).
        for (std::size_t i = 0; i + 1 < textCols_.size(); ++i) {
            list_->AppendColumn(textCols_[i].label, textCols_[i].format, textCols_[i].width);
        }
        // Icon columns.
        for (const auto& ic : iconCols_) {
            list_->AppendColumn("", wxLIST_FORMAT_CENTER, ic.width);
        }
        rebuildIconList(wxColour(20, 20, 20), wxColour(255, 255, 255));
        // Trailing Note column.
        const auto& last = textCols_.back();
        list_->AppendColumn(last.label, last.format, last.width);

        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(headerRow_, 0, wxEXPAND);
        sizer->Add(list_, 1, wxEXPAND);
        SetSizer(sizer);

        list_->Bind(wxEVT_LIST_ITEM_SELECTED,   &BaseCardListPanel::onSelectionChanged, this);
        list_->Bind(wxEVT_LIST_ITEM_DESELECTED, &BaseCardListPanel::onSelectionChanged, this);
    }

    // Forwarded helpers ------------------------------------------------------

    // wxListCtrl column indices for derived helpers.
    [[nodiscard]] int firstTextColIdx() const noexcept { return 1; }
    [[nodiscard]] int firstIconColIdx() const noexcept {
        return 1 + static_cast<int>(textCols_.size()) - 1;
    }
    [[nodiscard]] int noteColIdx() const noexcept {
        return firstIconColIdx() + static_cast<int>(iconCols_.size());
    }
    [[nodiscard]] int textColCount() const noexcept {
        return static_cast<int>(textCols_.size());
    }
    [[nodiscard]] int iconColCount() const noexcept {
        return static_cast<int>(iconCols_.size());
    }

    // Image-list indices for unselected (dark) and selected (light) variants.
    [[nodiscard]] int iconImgIdx (std::size_t i) const { return iconImgIdx_.at(i); }
    [[nodiscard]] int iconImgIdxW(std::size_t i) const { return iconImgIdxW_.at(i); }
    [[nodiscard]] wxListCtrl* listCtrl() const noexcept { return list_; }

    // Mutable access to the underlying vector for the typed `sortBy` hook
    // (the sort runs in-place on the same vector the base owns, so we can't
    // hand the subclass a copy).
    [[nodiscard]] std::vector<TCard>& mutableCards() noexcept { return cards_; }

private:
    // ----- header row construction -------------------------------------------

    void buildHeaderRow() {
        headerRow_ = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        auto* s = new wxBoxSizer(wxHORIZONTAL);
        headerCells_.clear();
        headerCellToCol_.clear();
        headerIcons_.clear();

        auto bindHeaderEvents = [this](wxWindow* hit, int col) {
            hit->Bind(wxEVT_LEFT_DOWN, [this, col](wxMouseEvent& ev) { onHeaderMouseDown(col, ev); });
            hit->Bind(wxEVT_MOTION,    [this, col](wxMouseEvent& ev) { onHeaderMouseMove(col, ev); });
            hit->Bind(wxEVT_LEFT_UP,   [this](wxMouseEvent& ev)      { onHeaderMouseUp(ev); });
            hit->Bind(wxEVT_LEFT_DCLICK,
                      [this, col](wxMouseEvent& ev) { onHeaderDoubleClick(col, ev); });
        };
        auto addText = [&](const wxString& label, int width, int col) {
            auto* p = new wxPanel(headerRow_, wxID_ANY, wxDefaultPosition, wxSize(width, -1), wxBORDER_NONE);
            auto* ps = new wxBoxSizer(wxHORIZONTAL);
            auto* t = new wxStaticText(p, wxID_ANY, label);
            ps->Add(t, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
            p->SetSizer(ps);
            p->SetMinSize(wxSize(width, -1));
            bindHeaderEvents(p, col);
            bindHeaderEvents(t, col);
            s->Add(p, 0, wxEXPAND);
            headerCells_.push_back(p);
            headerCellToCol_[p] = col;
        };
        auto addIcon = [&](const char* svg, int width, int col) {
            auto* p = new wxPanel(headerRow_, wxID_ANY, wxDefaultPosition, wxSize(width, -1), wxBORDER_NONE);
            auto* ps = new wxBoxSizer(wxHORIZONTAL);
            auto bmp = svgIconBitmap(svg, kFlagIconSize, "#E6E6E6");
            auto* b = new wxStaticBitmap(p, wxID_ANY, bmp);
            ps->AddStretchSpacer(1);
            ps->Add(b, 0, wxALIGN_CENTER_VERTICAL);
            ps->AddStretchSpacer(1);
            p->SetSizer(ps);
            p->SetMinSize(wxSize(width, -1));
            bindHeaderEvents(p, col);
            bindHeaderEvents(b, col);
            s->Add(p, 0, wxEXPAND);
            headerCells_.push_back(p);
            headerCellToCol_[p] = col;
            headerIcons_.push_back({b, svg});
        };

        const int firstText = firstTextColIdx();
        // Leading text columns.
        for (std::size_t i = 0; i + 1 < textCols_.size(); ++i) {
            addText(textCols_[i].label, textCols_[i].width, firstText + static_cast<int>(i));
        }
        const int firstIcon = firstIconColIdx();
        for (std::size_t i = 0; i < iconCols_.size(); ++i) {
            addIcon(iconCols_[i].svg, iconCols_[i].width, firstIcon + static_cast<int>(i));
        }
        const int noteCol = noteColIdx();
        addText(textCols_.back().label, textCols_.back().width, noteCol);

        headerRow_->SetSizer(s);
    }

    // ----- header drag-resize / sort hit-test ---------------------------------

    [[nodiscard]] bool isResizeGripHit(int col, int x) const {
        const int firstText = firstTextColIdx();
        if (col < firstText || col > noteColIdx()) return false;
        const std::size_t idx = static_cast<std::size_t>(col - firstText);
        if (idx >= headerCells_.size() || headerCells_[idx] == nullptr) return false;
        const int w = headerCells_[idx]->GetSize().GetWidth();
        return x >= (w - kResizeGripPx);
    }

    void setColumnWidth(int col, int width) {
        // Icon columns get a tighter min so they don't grow when dragged.
        const int firstIcon = firstIconColIdx();
        const int lastIcon  = firstIcon + iconColCount() - 1;
        const int minWidth = (col >= firstIcon && col <= lastIcon) ? 24 : 40;
        const int nextWidth = std::max(minWidth, width);
        list_->SetColumnWidth(col, nextWidth);
        const std::size_t idx = static_cast<std::size_t>(col - firstTextColIdx());
        if (idx < headerCells_.size() && headerCells_[idx] != nullptr) {
            headerCells_[idx]->SetMinSize(wxSize(nextWidth, -1));
        }
        headerRow_->Layout();
    }

    void autoSizeColumn(int col) {
        list_->SetColumnWidth(col, wxLIST_AUTOSIZE);
        const int contentWidth = list_->GetColumnWidth(col);
        list_->SetColumnWidth(col, wxLIST_AUTOSIZE_USEHEADER);
        const int headerWidth = list_->GetColumnWidth(col);
        setColumnWidth(col, std::max(contentWidth, headerWidth));
    }

    void autoSizeAllColumns() {
        for (int col = firstTextColIdx(); col <= noteColIdx(); ++col) {
            autoSizeColumn(col);
        }
    }

    void onHeaderMouseDown(int col, wxMouseEvent& ev) {
        wxWindow* src = dynamic_cast<wxWindow*>(ev.GetEventObject());
        wxWindow* cell = src;
        while (cell != nullptr && cell->GetParent() != headerRow_) {
            cell = cell->GetParent();
        }
        if (cell == nullptr) return;
        const wxPoint posInCell = cell->ScreenToClient(src->ClientToScreen(ev.GetPosition()));
        if (!isResizeGripHit(col, posInCell.x)) return;

        resizingCol_ = true;
        activeResizeCol_ = col;
        resizeStartScreenX_ = wxGetMousePosition().x;
        resizeStartWidth_ = list_->GetColumnWidth(col);
        cell->CaptureMouse();
    }

    void onHeaderMouseMove(int col, wxMouseEvent& ev) {
        wxWindow* src = dynamic_cast<wxWindow*>(ev.GetEventObject());
        wxWindow* cell = src;
        while (cell != nullptr && cell->GetParent() != headerRow_) {
            cell = cell->GetParent();
        }
        if (cell == nullptr) return;

        if (resizingCol_ && activeResizeCol_ == col && cell->HasCapture()) {
            const int delta = wxGetMousePosition().x - resizeStartScreenX_;
            setColumnWidth(col, resizeStartWidth_ + delta);
            return;
        }

        const wxPoint posInCell = cell->ScreenToClient(src->ClientToScreen(ev.GetPosition()));
        cell->SetCursor(isResizeGripHit(col, posInCell.x)
            ? wxCursor(wxCURSOR_SIZEWE)
            : wxCursor(wxCURSOR_ARROW));
    }

    void onHeaderMouseUp(wxMouseEvent& ev) {
        const bool wasResizing = resizingCol_;
        wxWindow* src = dynamic_cast<wxWindow*>(ev.GetEventObject());
        wxWindow* cell = src;
        while (cell != nullptr && cell->GetParent() != headerRow_) {
            cell = cell->GetParent();
        }
        if (cell != nullptr && cell->HasCapture()) {
            cell->ReleaseMouse();
        }
        resizingCol_ = false;
        if (suppressNextHeaderClick_) {
            suppressNextHeaderClick_ = false;
            activeResizeCol_ = -1;
            return;
        }
        if (!wasResizing && cell != nullptr) {
            auto it = headerCellToCol_.find(cell);
            if (it != headerCellToCol_.end()) {
                onHeaderClick(it->second);
            }
        }
        activeResizeCol_ = -1;
    }

    void onHeaderDoubleClick(int col, wxMouseEvent& ev) {
        wxWindow* src = dynamic_cast<wxWindow*>(ev.GetEventObject());
        wxWindow* cell = src;
        while (cell != nullptr && cell->GetParent() != headerRow_) {
            cell = cell->GetParent();
        }
        if (cell == nullptr) return;

        const wxPoint posInCell = cell->ScreenToClient(src->ClientToScreen(ev.GetPosition()));
        if (isResizeGripHit(col, posInCell.x)) {
            suppressNextHeaderClick_ = true;
            autoSizeColumn(col);
        }
    }

    // Map a physical wxListCtrl column to a sort column. Looks at the
    // declared TextColumnSpec/IconColumnSpec lists to find the optional
    // `sortColumn` for each column. Returns nullopt for non-sortable columns
    // (the spacer column 0 or any text/icon column without a sort key).
    [[nodiscard]] std::optional<TSortColumn> sortColumnForListIdx(int listColIdx) const {
        if (listColIdx <= 0) return std::nullopt;
        const int firstIcon = firstIconColIdx();
        const int noteCol   = noteColIdx();
        if (listColIdx < firstIcon) {
            const std::size_t i = static_cast<std::size_t>(listColIdx - firstTextColIdx());
            if (i < textCols_.size() - 1) return textCols_[i].sortColumn;
        } else if (listColIdx < noteCol) {
            const std::size_t i = static_cast<std::size_t>(listColIdx - firstIcon);
            if (i < iconCols_.size()) return iconCols_[i].sortColumn;
        } else if (listColIdx == noteCol) {
            return textCols_.back().sortColumn;
        }
        return std::nullopt;
    }

    void onHeaderClick(int col) {
        if (resizingCol_) return;
        const auto sortCol = sortColumnForListIdx(col);
        if (!sortCol) return;

        // Per-column toggle, faithful to TableTemplate.tsx::sortByField.
        auto it = nextDirByCol_.find(*sortCol);
        const bool ascending = (it == nextDirByCol_.end()) ? true : it->second;
        nextDirByCol_[*sortCol] = !ascending;

        std::optional<std::uint32_t> keepId;
        if (auto sel = selected()) keepId = sel->id;

        sortBy(*sortCol, ascending);
        rebuildRows(keepId);
    }

    // ----- icon image list ---------------------------------------------------

    void rebuildIconList(const wxColour& normal, const wxColour& selected) {
        const wxSize flagBmp(kFlagBmpWidth_, kFlagIconSize);
        auto* iconList = new wxImageList(flagBmp.GetWidth(), flagBmp.GetHeight(), /*hasMask=*/false);
        const std::string normalHex   = normal.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
        const std::string selectedHex = selected.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();

        iconImgIdx_.clear();
        iconImgIdxW_.clear();
        // Insert in two passes so the dark variants come first; some MSW code
        // paths assume the first N entries are the "normal" set.
        for (const auto& ic : iconCols_) {
            iconImgIdx_.push_back(
                iconList->Add(paddedSvgIcon(ic.svg, kFlagIconSize, flagBmp, normalHex.c_str())));
        }
        for (const auto& ic : iconCols_) {
            iconImgIdxW_.push_back(
                iconList->Add(paddedSvgIcon(ic.svg, kFlagIconSize, flagBmp, selectedHex.c_str())));
        }
        list_->AssignImageList(iconList, wxIMAGE_LIST_SMALL);

        const int firstIcon = firstIconColIdx();
        for (std::size_t i = 0; i < iconCols_.size(); ++i) {
            wxListItem hdr;
            hdr.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_IMAGE | wxLIST_MASK_FORMAT);
            hdr.SetText("");
            hdr.SetImage(iconImgIdx_[i]);
            hdr.SetAlign(wxLIST_FORMAT_CENTER);
            list_->SetColumn(firstIcon + static_cast<int>(i), hdr);
        }
    }

    void refreshHeaderTheme(const ThemePalette& palette) {
        headerRow_->SetBackgroundColour(palette.inputBg);
        headerRow_->SetForegroundColour(palette.inputText);
        headerRow_->SetOwnBackgroundColour(palette.inputBg);
        headerRow_->SetOwnForegroundColour(palette.inputText);
        for (wxWindow* cell : headerCells_) {
            if (cell == nullptr) continue;
            cell->SetBackgroundColour(palette.inputBg);
            cell->SetForegroundColour(palette.inputText);
            cell->SetOwnBackgroundColour(palette.inputBg);
            cell->SetOwnForegroundColour(palette.inputText);
            const wxWindowList& children = cell->GetChildren();
            for (wxWindowList::compatibility_iterator it = children.GetFirst(); it; it = it->GetNext()) {
                wxWindow* child = it->GetData();
                if (child == nullptr) continue;
                child->SetBackgroundColour(palette.inputBg);
                child->SetForegroundColour(palette.inputText);
                child->SetOwnBackgroundColour(palette.inputBg);
                child->SetOwnForegroundColour(palette.inputText);
            }
        }
        const std::string iconHex = palette.inputText.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
        for (auto& it : headerIcons_) {
            if (it.first == nullptr || it.second == nullptr) continue;
            it.first->SetBitmap(svgIconBitmap(it.second, kFlagIconSize, iconHex.c_str()));
        }
        headerRow_->Refresh();
    }

    // ----- row rendering -----------------------------------------------------

    void rebuildRows(std::optional<std::uint32_t> keepId = std::nullopt) {
        // Suppress wxListCtrl's natural DESELECTED (from DeleteAllItems) and
        // SELECTED (from the SetItemState below) events while we churn through
        // the rebuild. See `ui_wx/AGENTS.md` for the rate-limit rationale.
        inRebuild_ = true;
        list_->DeleteAllItems();

        filteredIndices_.clear();
        filteredIndices_.reserve(cards_.size());
        for (std::size_t i = 0; i < cards_.size(); ++i) {
            if (matchesFilter(cards_[i], filter_)) {
                filteredIndices_.push_back(i);
            }
        }

        auto setSubItemImage = [this](long row, int col, int imageIdx) {
            wxListItem li;
            li.SetId(row);
            li.SetColumn(col);
            li.SetMask(wxLIST_MASK_IMAGE);
            li.SetImage(imageIdx);
            list_->SetItem(li);
        };

        long row = 0;
        long rowToSelect = -1;
        const int firstText = firstTextColIdx();
        const int firstIcon = firstIconColIdx();
        const int noteCol   = noteColIdx();
        for (std::size_t srcIdx : filteredIndices_) {
            const auto& c = cards_[srcIdx];
            // Insert via the hidden column-0 spacer with image=-1 so MSW does
            // not implicitly draw the first image-list entry into column 0.
            wxListItem spacerItem;
            spacerItem.SetId(row);
            spacerItem.SetText("");
            spacerItem.SetImage(-1);
            spacerItem.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_IMAGE);
            const long idx = list_->InsertItem(spacerItem);

            // Leading text columns.
            for (std::size_t i = 0; i + 1 < textCols_.size(); ++i) {
                list_->SetItem(idx, firstText + static_cast<int>(i),
                               renderTextCell(c, i));
            }
            // Icon columns: empty cells for unset flags.
            for (std::size_t i = 0; i < iconCols_.size(); ++i) {
                if (isIconColumnSet(c, i)) {
                    setSubItemImage(idx, firstIcon + static_cast<int>(i), iconImgIdx_[i]);
                }
            }
            // Trailing Note text column.
            list_->SetItem(idx, noteCol, renderTextCell(c, textCols_.size() - 1));

            if (keepId && c.id == *keepId) rowToSelect = idx;
            ++row;
        }
        bool deferredInitialSelect = false;
        if (!filteredIndices_.empty() && rowToSelect >= 0) {
            list_->SetItemState(rowToSelect,
                                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            list_->EnsureVisible(rowToSelect);
        } else if (!filteredIndices_.empty() && !keepId.has_value()) {
            // Defer the initial selection to the next event turn so first
            // paint stays responsive.
            deferredInitialSelect = true;
            CallAfter([this]() {
                if (list_ == nullptr || list_->GetItemCount() <= 0) return;
                const long sel = list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
                if (sel >= 0) return;
                list_->SetItemState(0,
                                    wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                                    wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
                list_->EnsureVisible(0);
            });
        }

        inRebuild_ = false;
        if (!deferredInitialSelect) {
            notifySelectionChanged();
        }
    }

    void notifySelectionChanged() {
        // Fires the event on the panel itself. The owning IGameView binds
        // directly to its typed list panel so the typed selection wiring stays
        // local (MainFrame only sees IGameView, never MagicCard / PokemonCard).
        wxCommandEvent ev(EVT_CARD_SELECTED, GetId());
        ev.SetEventObject(this);
        ProcessWindowEvent(ev);
    }

    [[nodiscard]] const TCard* cardForRow(long row) const noexcept {
        if (row < 0) return nullptr;
        const auto r = static_cast<std::size_t>(row);
        if (r >= filteredIndices_.size()) return nullptr;
        const std::size_t srcIdx = filteredIndices_[r];
        if (srcIdx >= cards_.size()) return nullptr;
        return &cards_[srcIdx];
    }

    void applyRowSelectionStyle(long row, bool selected) {
        const TCard* c = cardForRow(row);
        if (!c) return;

        const int firstIcon = firstIconColIdx();
        for (std::size_t i = 0; i < iconCols_.size(); ++i) {
            if (!isIconColumnSet(*c, i)) continue;
            wxListItem li;
            li.SetId(row);
            li.SetColumn(firstIcon + static_cast<int>(i));
            li.SetMask(wxLIST_MASK_IMAGE);
            li.SetImage(selected ? iconImgIdxW_[i] : iconImgIdx_[i]);
            list_->SetItem(li);
        }
    }

    void onSelectionChanged(wxListEvent& event) {
        const long row     = event.GetIndex();
        const bool sel     = (event.GetEventType() == wxEVT_LIST_ITEM_SELECTED);
        applyRowSelectionStyle(row, sel);
        if (inRebuild_) return;
        notifySelectionChanged();
    }

    // ----- members ----------------------------------------------------------

    static constexpr int kFlagIconSize = 14;
    static constexpr int kResizeGripPx = 5;
    static constexpr int kFlagBmpWidth_ = 36 - 6;  // matches Magic legacy values

    wxPanel*               headerRow_{nullptr};
    std::vector<wxWindow*> headerCells_;
    std::vector<std::pair<wxStaticBitmap*, const char*>> headerIcons_;
    std::unordered_map<wxWindow*, int> headerCellToCol_;
    wxListCtrl*            list_{nullptr};

    std::vector<TextColumnSpec> textCols_;
    std::vector<IconColumnSpec> iconCols_;

    bool                   resizingCol_{false};
    bool                   suppressNextHeaderClick_{false};
    bool                   autoSizedOnce_{false};
    int                    activeResizeCol_{-1};
    int                    resizeStartScreenX_{0};
    int                    resizeStartWidth_{0};

    std::vector<TCard>          cards_;
    std::vector<std::size_t>    filteredIndices_;
    std::string                 filter_;

    // Rebuild guard - see ui_wx/AGENTS.md for the burst-suppression rationale.
    bool inRebuild_{false};

    std::map<TSortColumn, bool> nextDirByCol_;

    std::vector<int> iconImgIdx_;
    std::vector<int> iconImgIdxW_;
};

}  // namespace ccm::ui
