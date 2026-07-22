#include "ccm/ui/DigiBattle99SetCompletionPanel.hpp"

#include "ccm/services/DigiBattle99SetCompletion.hpp"

#include <wx/button.h>
#include <wx/cursor.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/scrolwin.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <utility>

namespace ccm::ui {

namespace {

wxColour mutedTextColour(const ThemePalette& palette) {
    // Blend text toward panel background so missing checklist rows read as greyed.
    const auto blend = [](unsigned char a, unsigned char b) -> unsigned char {
        return static_cast<unsigned char>((static_cast<int>(a) * 2 + static_cast<int>(b)) / 3);
    };
    return wxColour(blend(palette.text.Red(), palette.panelBg.Red()),
                    blend(palette.text.Green(), palette.panelBg.Green()),
                    blend(palette.text.Blue(), palette.panelBg.Blue()));
}

}  // namespace

DigiBattle99SetCompletionPanel::DigiBattle99SetCompletionPanel(
    wxWindow* parent, DigiBattle99SetCatalogService& catalogStore)
    : wxPanel(parent), catalogStore_(catalogStore) {
    palette_ = paletteForTheme(inferThemeFromWindow(this));

    book_ = new wxSimplebook(this, wxID_ANY);

    gridPage_ = new wxPanel(book_);
    auto* gridRoot = new wxBoxSizer(wxVERTICAL);
    emptyLabel_ = new wxStaticText(gridPage_, wxID_ANY, "");
    emptyLabel_->Wrap(480);
    gridRoot->Add(emptyLabel_, 0, wxALL | wxEXPAND, 12);

    scroll_ = new wxScrolledWindow(gridPage_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxVSCROLL | wxTAB_TRAVERSAL);
    scroll_->SetScrollRate(0, 16);
    gridSizer_ = new wxBoxSizer(wxVERTICAL);
    scroll_->SetSizer(gridSizer_);
    gridRoot->Add(scroll_, 1, wxEXPAND);
    gridPage_->SetSizer(gridRoot);
    book_->AddPage(gridPage_, "Grid");

    detailPage_ = new wxPanel(book_);
    auto* detailRoot = new wxBoxSizer(wxVERTICAL);
    auto* topRow = new wxBoxSizer(wxHORIZONTAL);
    auto* backBtn = new wxButton(detailPage_, wxID_ANY, "Back");
    backBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { showGridPage(); });
    detailTitle_ = new wxStaticText(detailPage_, wxID_ANY, "");
    auto titleFont = detailTitle_->GetFont();
    titleFont.MakeBold().MakeLarger();
    detailTitle_->SetFont(titleFont);
    topRow->Add(backBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    topRow->Add(detailTitle_, 1, wxALIGN_CENTER_VERTICAL);
    detailRoot->Add(topRow, 0, wxEXPAND | wxALL, 8);

    checklist_ = new wxListCtrl(detailPage_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);
    checklist_->AppendColumn("Card", wxLIST_FORMAT_LEFT, 520);
    detailRoot->Add(checklist_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    detailPage_->SetSizer(detailRoot);
    book_->AddPage(detailPage_, "Detail");

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(book_, 1, wxEXPAND);
    SetSizer(root);

    showGridPage();
}

void DigiBattle99SetCompletionPanel::setCollection(std::vector<DigiBattle99Card> cards) {
    collection_ = std::move(cards);
    if (book_->GetSelection() == 1 && !detailSetId_.empty()) {
        rebuildChecklist(detailSetId_);
    } else {
        rebuildGrid();
    }
}

void DigiBattle99SetCompletionPanel::reloadFromStore() {
    catalogLoaded_ = false;
    catalog_ = {};
    if (catalogStore_.exists()) {
        if (auto loaded = catalogStore_.load()) {
            catalog_ = std::move(loaded).value();
            catalogLoaded_ = true;
        }
    }
    showGridPage();
    rebuildGrid();
}

void DigiBattle99SetCompletionPanel::applyTheme(const ThemePalette& palette) {
    palette_ = palette;
    applyThemeToWindowTree(this, palette, inferThemeFromWindow(this));
    if (book_->GetSelection() == 1 && !detailSetId_.empty()) {
        rebuildChecklist(detailSetId_);
    } else {
        rebuildGrid();
    }
}

void DigiBattle99SetCompletionPanel::showGridPage() {
    detailSetId_.clear();
    book_->SetSelection(0);
}

void DigiBattle99SetCompletionPanel::showChecklistPage(const std::string& setId,
                                                      const std::string& setName) {
    detailSetId_ = setId;
    detailTitle_->SetLabelText(wxString::FromUTF8(setName.c_str()));
    rebuildChecklist(setId);
    book_->SetSelection(1);
}

void DigiBattle99SetCompletionPanel::setEmptyMessage(const wxString& message) {
    clearGridTiles();
    emptyLabel_->SetLabelText(message);
    emptyLabel_->Wrap(480);
    emptyLabel_->Show();
    scroll_->Hide();
    gridPage_->Layout();
}

void DigiBattle99SetCompletionPanel::clearGridTiles() {
    if (gridSizer_ == nullptr) return;
    gridSizer_->Clear(true);
}

void DigiBattle99SetCompletionPanel::rebuildGrid() {
    if (!catalogLoaded_) {
        setEmptyMessage(wxString::FromUTF8(
            "Set checklists are not downloaded yet.\n"
            "Run Sets → Update Digimon (Digi-Battle) to enable Set Completion."));
        return;
    }

    const auto rows = computeDigiBattle99SetCompletion(collection_, catalog_);
    if (rows.empty()) {
        setEmptyMessage(wxString::FromUTF8(
            "No Digimon (Digi-Battle) sets in progress yet.\n"
            "Add cards on the Single Cards tab to track set completion here."));
        return;
    }

    emptyLabel_->Hide();
    scroll_->Show();
    clearGridTiles();

    for (const auto& row : rows) {
        auto* tile = new wxPanel(scroll_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxBORDER_SIMPLE);
        tile->SetBackgroundColour(palette_.panelBg);
        auto* tileSizer = new wxBoxSizer(wxVERTICAL);

        auto* nameLbl = new wxStaticText(tile, wxID_ANY, wxString::FromUTF8(row.setName.c_str()));
        auto nameFont = nameLbl->GetFont();
        nameFont.MakeBold();
        nameLbl->SetFont(nameFont);
        nameLbl->SetForegroundColour(palette_.text);

        const std::string counts =
            std::to_string(row.ownedUnique) + " / " + std::to_string(row.total) + "  (" +
            std::to_string(row.percent()) + "%)";
        auto* countLbl = new wxStaticText(tile, wxID_ANY, wxString::FromUTF8(counts.c_str()));
        countLbl->SetForegroundColour(palette_.text);

        auto* gauge = new wxGauge(tile, wxID_ANY, 100, wxDefaultPosition, wxSize(-1, 14),
                                  wxGA_HORIZONTAL | wxGA_SMOOTH);
        gauge->SetValue(row.percent());

        tileSizer->Add(nameLbl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
        tileSizer->Add(countLbl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
        tileSizer->Add(gauge, 0, wxEXPAND | wxALL, 10);
        tile->SetSizer(tileSizer);

        const std::string setId = row.setId;
        const std::string setName = row.setName;
        auto openDetail = [this, setId, setName](wxMouseEvent&) {
            showChecklistPage(setId, setName);
        };
        tile->Bind(wxEVT_LEFT_UP, openDetail);
        nameLbl->Bind(wxEVT_LEFT_UP, openDetail);
        countLbl->Bind(wxEVT_LEFT_UP, openDetail);
        gauge->Bind(wxEVT_LEFT_UP, openDetail);
        tile->SetCursor(wxCursor(wxCURSOR_HAND));
        nameLbl->SetCursor(wxCursor(wxCURSOR_HAND));
        countLbl->SetCursor(wxCursor(wxCURSOR_HAND));

        gridSizer_->Add(tile, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    }
    gridSizer_->AddStretchSpacer(1);
    scroll_->FitInside();
    gridPage_->Layout();
    Layout();
}

void DigiBattle99SetCompletionPanel::rebuildChecklist(const std::string& setId) {
    checklist_->DeleteAllItems();
    const auto entries = digiBattle99ChecklistForSet(collection_, catalog_, setId);
    const wxColour muted = mutedTextColour(palette_);

    long idx = 0;
    for (const auto& entry : entries) {
        const std::string line = entry.setNo + "  —  " + entry.name;
        const long row = checklist_->InsertItem(idx++, wxString::FromUTF8(line.c_str()));
        if (row < 0) continue;
        if (entry.owned) {
            checklist_->SetItemTextColour(row, palette_.text);
        } else {
            checklist_->SetItemTextColour(row, muted);
        }
    }
    checklist_->SetColumnWidth(0, wxLIST_AUTOSIZE);
    detailPage_->Layout();
}

}  // namespace ccm::ui
