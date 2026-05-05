#include "ccm/ui/MagicCardListPanel.hpp"

#include "ccm/services/CardFilter.hpp"
#include "ccm/ui/SvgIcons.hpp"

#include <wx/colour.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/utils.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace ccm::ui {

wxDEFINE_EVENT(EVT_MAGIC_CARD_SELECTED, wxCommandEvent);

namespace {
// Column indices. Column 0 is a hidden zero-width spacer that absorbs the
// fixed item-icon area MSW reserves at the left of every row whenever a
// small image list is attached. Without it, column 0's icon slot leaves a
// visible gap to the left of the first real column even when iImage = -1.
// Foil / Signed / Altered are icon-only flag columns; Note tails the row.
constexpr int kColSpacer    = 0;
constexpr int kColName      = 1;
constexpr int kColSet       = 2;
constexpr int kColAmount    = 3;
constexpr int kColCondition = 4;
constexpr int kColLanguage  = 5;
constexpr int kColFoil      = 6;
constexpr int kColSigned    = 7;
constexpr int kColAltered   = 8;
constexpr int kColNote      = 9;

constexpr int kFlagIconSize = 14;  // matches the side panel
constexpr int kFlagColWidth = 36;
// Image-list bitmaps are sized to the column's content area (column width
// minus the comctl32 ~6px header padding) so the centered icon ends up
// visually centered under the header AND inside each cell.
constexpr int kFlagBmpWidth = kFlagColWidth - 6;
constexpr int kResizeGripPx = 5;
}  // namespace

MagicCardListPanel::MagicCardListPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {
    list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);

    buildHeaderRow();

    // Column 0 is a hidden spacer that swallows MSW's mandatory item-icon
    // gutter. All real columns live at index >= 1.
    list_->AppendColumn("", wxLIST_FORMAT_LEFT, 0);

    list_->AppendColumn("Name",      wxLIST_FORMAT_LEFT,  220);
    list_->AppendColumn("Set",       wxLIST_FORMAT_LEFT,  180);
    list_->AppendColumn("Amount",    wxLIST_FORMAT_RIGHT, 70);
    list_->AppendColumn("Condition", wxLIST_FORMAT_LEFT,  100);
    list_->AppendColumn("Language",  wxLIST_FORMAT_LEFT,  100);

    list_->AppendColumn("", wxLIST_FORMAT_CENTER, kFlagColWidth);
    list_->AppendColumn("", wxLIST_FORMAT_CENTER, kFlagColWidth);
    list_->AppendColumn("", wxLIST_FORMAT_CENTER, kFlagColWidth);
    rebuildIconList(wxColour(20, 20, 20), wxColour(255, 255, 255));

    list_->AppendColumn("Note", wxLIST_FORMAT_LEFT, 220);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(headerRow_, 0, wxEXPAND);
    sizer->Add(list_, 1, wxEXPAND);
    SetSizer(sizer);

    list_->Bind(wxEVT_LIST_ITEM_SELECTED,   &MagicCardListPanel::onSelectionChanged, this);
    list_->Bind(wxEVT_LIST_ITEM_DESELECTED, &MagicCardListPanel::onSelectionChanged, this);
}

void MagicCardListPanel::buildHeaderRow() {
    headerRow_ = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    auto* s = new wxBoxSizer(wxHORIZONTAL);
    headerCells_.clear();
    headerCellToCol_.clear();
    headerIcons_.clear();

    auto addText = [&](const wxString& label, int width, int col) {
        auto* p = new wxPanel(headerRow_, wxID_ANY, wxDefaultPosition, wxSize(width, -1), wxBORDER_NONE);
        auto* ps = new wxBoxSizer(wxHORIZONTAL);
        auto* t = new wxStaticText(p, wxID_ANY, label);
        ps->Add(t, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
        p->SetSizer(ps);
        p->SetMinSize(wxSize(width, -1));
        p->Bind(wxEVT_LEFT_DOWN, [this, col](wxMouseEvent& ev) { onHeaderMouseDown(col, ev); });
        p->Bind(wxEVT_MOTION, [this, col](wxMouseEvent& ev) { onHeaderMouseMove(col, ev); });
        p->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& ev) { onHeaderMouseUp(ev); });
        p->Bind(wxEVT_LEFT_DCLICK, [this, col](wxMouseEvent& ev) { onHeaderDoubleClick(col, ev); });
        t->Bind(wxEVT_LEFT_DOWN, [this, col](wxMouseEvent& ev) { onHeaderMouseDown(col, ev); });
        t->Bind(wxEVT_MOTION, [this, col](wxMouseEvent& ev) { onHeaderMouseMove(col, ev); });
        t->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& ev) { onHeaderMouseUp(ev); });
        t->Bind(wxEVT_LEFT_DCLICK, [this, col](wxMouseEvent& ev) { onHeaderDoubleClick(col, ev); });
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
        p->Bind(wxEVT_LEFT_DOWN, [this, col](wxMouseEvent& ev) { onHeaderMouseDown(col, ev); });
        p->Bind(wxEVT_MOTION, [this, col](wxMouseEvent& ev) { onHeaderMouseMove(col, ev); });
        p->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& ev) { onHeaderMouseUp(ev); });
        p->Bind(wxEVT_LEFT_DCLICK, [this, col](wxMouseEvent& ev) { onHeaderDoubleClick(col, ev); });
        b->Bind(wxEVT_LEFT_DOWN, [this, col](wxMouseEvent& ev) { onHeaderMouseDown(col, ev); });
        b->Bind(wxEVT_MOTION, [this, col](wxMouseEvent& ev) { onHeaderMouseMove(col, ev); });
        b->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& ev) { onHeaderMouseUp(ev); });
        b->Bind(wxEVT_LEFT_DCLICK, [this, col](wxMouseEvent& ev) { onHeaderDoubleClick(col, ev); });
        s->Add(p, 0, wxEXPAND);
        headerCells_.push_back(p);
        headerCellToCol_[p] = col;
        headerIcons_.push_back({b, svg});
    };

    addText("Name", 220, kColName);
    addText("Set", 180, kColSet);
    addText("Amount", 70, kColAmount);
    addText("Condition", 100, kColCondition);
    addText("Language", 100, kColLanguage);
    addIcon(kSvgFoil, kFlagColWidth, kColFoil);
    addIcon(kSvgSigned, kFlagColWidth, kColSigned);
    addIcon(kSvgAltered, kFlagColWidth, kColAltered);
    addText("Note", 220, kColNote);

    headerRow_->SetSizer(s);
}

bool MagicCardListPanel::isResizeGripHit(int col, int x) const {
    if (col < kColName || col > kColNote) return false;
    const std::size_t idx = static_cast<std::size_t>(col - kColName);
    if (idx >= headerCells_.size() || headerCells_[idx] == nullptr) return false;
    const int w = headerCells_[idx]->GetSize().GetWidth();
    return x >= (w - kResizeGripPx);
}

void MagicCardListPanel::setColumnWidth(int col, int width) {
    int minWidth = 40;
    if (col == kColFoil || col == kColSigned || col == kColAltered) minWidth = 24;
    const int nextWidth = std::max(minWidth, width);
    list_->SetColumnWidth(col, nextWidth);
    const std::size_t idx = static_cast<std::size_t>(col - kColName);
    if (idx < headerCells_.size() && headerCells_[idx] != nullptr) {
        headerCells_[idx]->SetMinSize(wxSize(nextWidth, -1));
    }
    headerRow_->Layout();
}

void MagicCardListPanel::autoSizeColumn(int col) {
    // Match native list-view behavior on divider double-click: width should
    // fit the widest row content OR the header label, whichever is larger.
    list_->SetColumnWidth(col, wxLIST_AUTOSIZE);
    const int contentWidth = list_->GetColumnWidth(col);
    list_->SetColumnWidth(col, wxLIST_AUTOSIZE_USEHEADER);
    const int headerWidth = list_->GetColumnWidth(col);
    setColumnWidth(col, std::max(contentWidth, headerWidth));
}

void MagicCardListPanel::autoSizeAllColumns() {
    // Skip the hidden spacer column (0). Auto-fit user-visible columns.
    for (int col = kColName; col <= kColNote; ++col) {
        autoSizeColumn(col);
    }
}

void MagicCardListPanel::onHeaderMouseDown(int col, wxMouseEvent& ev) {
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

void MagicCardListPanel::onHeaderMouseMove(int col, wxMouseEvent& ev) {
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

void MagicCardListPanel::onHeaderMouseUp(wxMouseEvent& ev) {
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

void MagicCardListPanel::onHeaderDoubleClick(int col, wxMouseEvent& ev) {
    wxWindow* src = dynamic_cast<wxWindow*>(ev.GetEventObject());
    wxWindow* cell = src;
    while (cell != nullptr && cell->GetParent() != headerRow_) {
        cell = cell->GetParent();
    }
    if (cell == nullptr) return;

    const wxPoint posInCell = cell->ScreenToClient(src->ClientToScreen(ev.GetPosition()));
    // Double-click near divider edge should auto-size, and must not sort.
    if (isResizeGripHit(col, posInCell.x)) {
        suppressNextHeaderClick_ = true;
        autoSizeColumn(col);
    }
}

void MagicCardListPanel::rebuildIconList(const wxColour& normal, const wxColour& selected) {
    const wxSize flagBmp(kFlagBmpWidth, kFlagIconSize);
    auto* iconList = new wxImageList(flagBmp.GetWidth(), flagBmp.GetHeight(),
                                     /*hasMask=*/false);
    const std::string normalHex = normal.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
    const std::string selectedHex = selected.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();

    foilImgIdx_    = iconList->Add(paddedSvgIcon(kSvgFoil,    kFlagIconSize, flagBmp, normalHex.c_str()));
    signedImgIdx_  = iconList->Add(paddedSvgIcon(kSvgSigned,  kFlagIconSize, flagBmp, normalHex.c_str()));
    alteredImgIdx_ = iconList->Add(paddedSvgIcon(kSvgAltered, kFlagIconSize, flagBmp, normalHex.c_str()));
    foilImgIdxW_    = iconList->Add(paddedSvgIcon(kSvgFoil,    kFlagIconSize, flagBmp, selectedHex.c_str()));
    signedImgIdxW_  = iconList->Add(paddedSvgIcon(kSvgSigned,  kFlagIconSize, flagBmp, selectedHex.c_str()));
    alteredImgIdxW_ = iconList->Add(paddedSvgIcon(kSvgAltered, kFlagIconSize, flagBmp, selectedHex.c_str()));
    list_->AssignImageList(iconList, wxIMAGE_LIST_SMALL);

    auto setIconHeader = [this](int col, int imageIdx) {
        wxListItem hdr;
        hdr.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_IMAGE | wxLIST_MASK_FORMAT);
        hdr.SetText("");
        hdr.SetImage(imageIdx);
        hdr.SetAlign(wxLIST_FORMAT_CENTER);
        list_->SetColumn(col, hdr);
    };
    setIconHeader(kColFoil,    foilImgIdx_);
    setIconHeader(kColSigned,  signedImgIdx_);
    setIconHeader(kColAltered, alteredImgIdx_);
}

void MagicCardListPanel::applyTheme(const ThemePalette& palette) {
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

void MagicCardListPanel::refreshHeaderTheme(const ThemePalette& palette) {
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

void MagicCardListPanel::setCards(std::vector<MagicCard> cards) {
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

void MagicCardListPanel::rebuildRows(std::optional<std::uint32_t> keepId) {
    // Suppress the wxListCtrl's natural DESELECTED (from DeleteAllItems) and
    // SELECTED (from the SetItemState below) events while we churn through
    // the rebuild. Without this guard each keystroke in the filter input
    // would propagate 2 EVT_MAGIC_CARD_SELECTED events to the parent, each
    // of which spawns a Scryfall preview fetch (search + image download).
    // A few keystrokes in a row easily trip Scryfall's ~10 req/s soft limit
    // and previews start failing with HTTP 429.
    inRebuild_ = true;
    list_->DeleteAllItems();

    // Recompute the filtered view from the current `cards_` order and the
    // current filter string. The table logic evaluates `applyFilter` on every render —
    // we do the equivalent on every rebuild (i.e. setCards / sort / typing
    // in the filter input).
    filteredIndices_.clear();
    filteredIndices_.reserve(cards_.size());
    for (std::size_t i = 0; i < cards_.size(); ++i) {
        if (matchesMagicFilter(cards_[i], filter_)) {
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
    for (std::size_t srcIdx : filteredIndices_) {
        const auto& c = cards_[srcIdx];
        // Insert via the hidden column-0 spacer with image=-1. The Name and
        // every other visible field is filled in afterwards through SetItem
        // on the appropriate sub-item column.
        wxListItem spacerItem;
        spacerItem.SetId(row);
        spacerItem.SetText("");
        spacerItem.SetImage(-1);
        spacerItem.SetMask(wxLIST_MASK_TEXT | wxLIST_MASK_IMAGE);
        const long idx = list_->InsertItem(spacerItem);

        list_->SetItem(idx, kColName,      c.name);
        list_->SetItem(idx, kColSet,       c.set.name);
        list_->SetItem(idx, kColAmount,    std::to_string(c.amount));
        list_->SetItem(idx, kColCondition, std::string(to_string(c.condition)));
        list_->SetItem(idx, kColLanguage,  std::string(to_string(c.language)));

        // Empty cells for unset flags; setting -1 leaves them blank rather
        // than reusing whatever image happened to be at index 0.
        if (c.foil)    setSubItemImage(idx, kColFoil,    foilImgIdx_);
        if (c.signed_) setSubItemImage(idx, kColSigned,  signedImgIdx_);
        if (c.altered) setSubItemImage(idx, kColAltered, alteredImgIdx_);

        list_->SetItem(idx, kColNote, c.note);

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
        // Startup path: keep first-row auto-selection semantics, but defer
        // the actual selection to the next event turn so initial rendering
        // stays responsive.
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

    // Lift the suppression and emit exactly one settled event so the parent
    // resyncs (preview panel, status bar, etc.) regardless of whether the
    // filtered list ended up empty or not.
    inRebuild_ = false;
    if (!deferredInitialSelect) {
        notifySelectionChanged();
    }
}

void MagicCardListPanel::notifySelectionChanged() {
    wxCommandEvent ev(EVT_MAGIC_CARD_SELECTED, GetId());
    ev.SetEventObject(this);
    if (auto* parent = GetParent()) {
        parent->GetEventHandler()->ProcessEvent(ev);
    } else {
        ProcessWindowEvent(ev);
    }
}

const MagicCard* MagicCardListPanel::cardForRow(long row) const noexcept {
    if (row < 0) return nullptr;
    const auto r = static_cast<std::size_t>(row);
    if (r >= filteredIndices_.size()) return nullptr;
    const std::size_t srcIdx = filteredIndices_[r];
    if (srcIdx >= cards_.size()) return nullptr;
    return &cards_[srcIdx];
}

std::optional<MagicCard> MagicCardListPanel::selected() const {
    const long sel = list_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (const MagicCard* c = cardForRow(sel)) return *c;
    return std::nullopt;
}

void MagicCardListPanel::applyRowSelectionStyle(long row, bool selected) {
    const MagicCard* c = cardForRow(row);
    if (!c) return;

    auto setImg = [this, row](int col, int imgIdx) {
        wxListItem li;
        li.SetId(row);
        li.SetColumn(col);
        li.SetMask(wxLIST_MASK_IMAGE);
        li.SetImage(imgIdx);
        list_->SetItem(li);
    };

    if (c->foil)    setImg(kColFoil,    selected ? foilImgIdxW_    : foilImgIdx_);
    if (c->signed_) setImg(kColSigned,  selected ? signedImgIdxW_  : signedImgIdx_);
    if (c->altered) setImg(kColAltered, selected ? alteredImgIdxW_ : alteredImgIdx_);
}

void MagicCardListPanel::onHeaderClick(int col) {
    if (resizingCol_) return;
    // Map the physical wxListCtrl column index back to a MagicSortColumn.
    // Column 0 is the hidden spacer described in the file header; ignore it.
    // Note: this map mirrors the AppendColumn calls in the constructor; it
    // must be updated in lockstep if the column layout changes.
    std::optional<MagicSortColumn> sortCol;
    switch (col) {
    case kColName:      sortCol = MagicSortColumn::Name;           break;
    case kColSet:       sortCol = MagicSortColumn::SetReleaseDate; break;
    case kColAmount:    sortCol = MagicSortColumn::Amount;         break;
    case kColCondition: sortCol = MagicSortColumn::Condition;      break;
    case kColLanguage:  sortCol = MagicSortColumn::Language;       break;
    case kColFoil:      sortCol = MagicSortColumn::Foil;           break;
    case kColSigned:    sortCol = MagicSortColumn::Signed;         break;
    case kColAltered:   sortCol = MagicSortColumn::Altered;        break;
    case kColNote:      sortCol = MagicSortColumn::Note;           break;
    default: return;
    }

    // Per-column toggle, faithful to TableTemplate.tsx::sortByField:
    // the entry stored in nextDirByCol_ is the direction to use on the *next*
    // click of that column. Default (absent entry) is ascending.
    auto it = nextDirByCol_.find(*sortCol);
    const bool ascending = (it == nextDirByCol_.end()) ? true : it->second;
    nextDirByCol_[*sortCol] = !ascending;

    // Preserve the user's selection across the sort by remembering the id of
    // the currently-selected card and re-selecting it after rebuildRows().
    std::optional<std::uint32_t> keepId;
    if (auto sel = selected()) keepId = sel->id;

    sortMagicCards(cards_, *sortCol, ascending);
    rebuildRows(keepId);
}

void MagicCardListPanel::setFilter(std::string_view filter) {
    // No-op early out keeps idle keystrokes (e.g. arrow keys in the input)
    // from triggering pointless wxListCtrl rebuilds and the redundant
    // EVT_MAGIC_CARD_SELECTED that follows.
    if (filter_ == filter) return;
    filter_.assign(filter);
    // Preserve the user's selection if the previously-selected card still
    // matches the new filter. Same `keepId`-by-card-id pattern as sort.
    // rebuildRows emits a single settled EVT_MAGIC_CARD_SELECTED at the end,
    // so the side panel re-syncs without us bubbling a redundant one here.
    std::optional<std::uint32_t> keepId;
    if (auto sel = selected()) keepId = sel->id;
    rebuildRows(keepId);
}

void MagicCardListPanel::onSelectionChanged(wxListEvent& event) {
    const long row     = event.GetIndex();
    const bool selected = (event.GetEventType() == wxEVT_LIST_ITEM_SELECTED);
    applyRowSelectionStyle(row, selected);

    // Sort and filter rebuilds emit their own settled selection event at the
    // end of rebuildRows(); the per-row SELECTED/DESELECTED noise from
    // SetItemState / DeleteAllItems must not bubble or the side panel re-
    // fetches the same Scryfall preview multiple times per keystroke.
    if (inRebuild_) return;

    notifySelectionChanged();
}

}  // namespace ccm::ui
