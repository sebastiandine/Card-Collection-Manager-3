#pragma once

// MagicCardListPanel: wxListCtrl-backed table of MagicCard rows.
// Notifies the parent frame of selection changes via a wxCommandEvent.
// Column headers are clickable to sort the rows; the per-column toggle
// behavior matches TableTemplate.tsx (first click asc, subsequent
// clicks toggle direction; switching columns starts ascending).
//
// Free-text filtering mirrors TableTemplate.tsx::applyFilter — the
// caller drives the current filter string via `setFilter(...)`. The panel
// re-evaluates which rows to show using ccm::matchesMagicFilter
// (case-insensitive substring match across name, set.name, language,
// condition, amount, note). The actual filter input widget lives outside
// this panel (in MainFrame) so layout decisions stay with the frame.

#include "ccm/domain/MagicCard.hpp"
#include "ccm/services/CardSorter.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/event.h>
#include <wx/listctrl.h>
#include <wx/panel.h>

class wxStaticBitmap;

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ccm::ui {

wxDECLARE_EVENT(EVT_MAGIC_CARD_SELECTED, wxCommandEvent);

class MagicCardListPanel : public wxPanel {
public:
    MagicCardListPanel(wxWindow* parent);

    // Replace the displayed rows. Selection is reset.
    void setCards(std::vector<MagicCard> cards);

    // Update the filter string and rebuild the visible rows in place. The
    // panel preserves the previously-selected card across the rebuild when
    // it still matches the new filter; otherwise the first remaining row is
    // selected, or none if the filter excluded everything. A single
    // EVT_MAGIC_CARD_SELECTED is emitted afterwards so the parent re-syncs.
    void setFilter(std::string_view filter);
    void applyTheme(const ThemePalette& palette);

    [[nodiscard]] const std::vector<MagicCard>& cards() const noexcept { return cards_; }
    [[nodiscard]] const std::string&            filter() const noexcept { return filter_; }
    [[nodiscard]] std::optional<MagicCard>      selected() const;

private:
    void buildHeaderRow();
    void onHeaderClick(int col);
    void onHeaderMouseDown(int col, wxMouseEvent& ev);
    void onHeaderMouseMove(int col, wxMouseEvent& ev);
    void onHeaderMouseUp(wxMouseEvent& ev);
    void onHeaderDoubleClick(int col, wxMouseEvent& ev);
    void setColumnWidth(int col, int width);
    void autoSizeColumn(int col);
    void autoSizeAllColumns();
    [[nodiscard]] bool isResizeGripHit(int col, int x) const;
    void onSelectionChanged(wxListEvent&);
    // Repaint the foil/signed/altered cells of `row` using either the
    // dark-on-light or the light-on-dark icon variant, mirroring how the
    // wxListCtrl renders text in the selected vs. unselected state.
    void applyRowSelectionStyle(long row, bool selected);
    // Recompute filteredIndices_ from cards_ + filter_, then refresh the
    // wxListCtrl to mirror that view, optionally re-selecting the row whose
    // MagicCard.id matches `keepId`.
    void rebuildRows(std::optional<std::uint32_t> keepId = std::nullopt);
    // Map a wxListCtrl row index to the underlying MagicCard via
    // filteredIndices_. Returns nullptr if `row` is out of range.
    [[nodiscard]] const MagicCard* cardForRow(long row) const noexcept;
    // Send EVT_MAGIC_CARD_SELECTED up to the parent's event handler. Used
    // by `rebuildRows` after the suppressed-during-rebuild burst settles.
    void notifySelectionChanged();
    void rebuildIconList(const wxColour& normal, const wxColour& selected);
    void refreshHeaderTheme(const ThemePalette& palette);

    wxPanel*               headerRow_{nullptr};
    std::vector<wxWindow*> headerCells_;
    std::vector<std::pair<wxStaticBitmap*, const char*>> headerIcons_;
    std::unordered_map<wxWindow*, int> headerCellToCol_;
    wxListCtrl*            list_{nullptr};
    bool                   resizingCol_{false};
    bool                   suppressNextHeaderClick_{false};
    bool                   autoSizedOnce_{false};
    int                    activeResizeCol_{-1};
    int                    resizeStartScreenX_{0};
    int                    resizeStartWidth_{0};
    // Full collection in display/sort order. The wxListCtrl shows a subset of
    // this, indexed by filteredIndices_.
    std::vector<MagicCard> cards_;
    std::vector<std::size_t> filteredIndices_;
    // Current filter string driven externally via setFilter(). Empty means
    // every row passes (parity: "".includes(filter) is true for "").
    std::string            filter_;

    // While rebuilding the list, wxListCtrl naturally fires DESELECTED (from
    // DeleteAllItems) and SELECTED (from the SetItemState we issue at the
    // end). Both want to bubble EVT_MAGIC_CARD_SELECTED up to the parent,
    // which would spawn redundant Scryfall preview fetches and trip the API
    // rate limit. We suppress them while this flag is true and emit a single
    // settled event from rebuildRows() once everything is in place.
    bool inRebuild_{false};

    // Sort state. `nextDirByCol_` stores the direction to use on the *next*
    // click of each column - same shape as `sortOrderByField`. After
    // a sort runs we flip the entry for that column. Columns absent from
    // the map default to ascending (true) on first click.
    std::map<MagicSortColumn, bool> nextDirByCol_;

    // Image indices into the wxListCtrl's small-icon list. The dark variants
    // are used for column header glyphs and unselected rows; the light
    // variants swap in for the currently-selected row so the icons stay
    // legible against the system highlight color.
    int foilImgIdx_{-1};
    int signedImgIdx_{-1};
    int alteredImgIdx_{-1};
    int foilImgIdxW_{-1};
    int signedImgIdxW_{-1};
    int alteredImgIdxW_{-1};
};

}  // namespace ccm::ui
