#include "ccm/ui/YuGiOhBandaiSetCompletionPanel.hpp"

#include "ccm/games/yugiohbandai/YuGiOhBandaiSetSource.hpp"
#include "ccm/services/YuGiOhBandaiSetCompletion.hpp"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/cursor.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/scrolwin.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <string>
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

YuGiOhBandaiSetCompletionPanel::YuGiOhBandaiSetCompletionPanel(
    wxWindow* parent, YuGiOhBandaiSetCatalogService& catalogStore)
    : wxPanel(parent), catalogStore_(catalogStore) {
    palette_ = paletteForTheme(inferThemeFromWindow(this));

    auto* langRow = new wxBoxSizer(wxHORIZONTAL);
    auto* langLabel = new wxStaticText(this, wxID_ANY, "Language");
    languageChoice_ = new wxChoice(this, wxID_ANY);
    languageChoice_->Append("All languages");
    languageChoice_->SetSelection(0);
    languageChoice_->Bind(wxEVT_CHOICE, &YuGiOhBandaiSetCompletionPanel::onLanguageChoice,
                          this);
    langRow->Add(langLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    langRow->Add(languageChoice_, 0, wxALIGN_CENTER_VERTICAL);

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
    root->Add(langRow, 0, wxEXPAND | wxALL, 8);
    root->Add(book_, 1, wxEXPAND);
    SetSizer(root);

    showGridPage();
}

void YuGiOhBandaiSetCompletionPanel::setCollection(std::vector<YuGiOhBandaiCard> cards) {
    collection_ = std::move(cards);
    refreshLanguageChoice();
    rebuildCurrentView();
}

void YuGiOhBandaiSetCompletionPanel::reloadFromStore() {
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

void YuGiOhBandaiSetCompletionPanel::applyTheme(const ThemePalette& palette) {
    palette_ = palette;
    applyThemeToWindowTree(this, palette, inferThemeFromWindow(this));
    rebuildCurrentView();
}

void YuGiOhBandaiSetCompletionPanel::showGridPage() {
    detailSetId_.clear();
    detailSetName_.clear();
    book_->SetSelection(0);
}

void YuGiOhBandaiSetCompletionPanel::showChecklistPage(const std::string& setId,
                                                       const std::string& setName) {
    detailSetId_ = setId;
    detailSetName_ = setName;
    detailTitle_->SetLabelText(wxString::FromUTF8(displaySetName(setName).c_str()));
    rebuildChecklist(setId);
    book_->SetSelection(1);
}

std::string YuGiOhBandaiSetCompletionPanel::displaySetName(const std::string& setName) const {
    if (!languageFilter_.has_value()) return setName;
    return setName + " (" + std::string(to_string(*languageFilter_)) + ")";
}

void YuGiOhBandaiSetCompletionPanel::refreshLanguageChoice() {
    const auto previous = languageFilter_;
    const auto present = yuGiOhBandaiLanguagesInCollection(collection_);

    languageChoice_->Clear();
    languageChoice_->Append("All languages");
    for (const Language lang : present) {
        languageChoice_->Append(wxString::FromUTF8(std::string(to_string(lang)).c_str()));
    }

    int selection = 0;
    languageFilter_ = std::nullopt;
    if (previous.has_value()) {
        for (std::size_t i = 0; i < present.size(); ++i) {
            if (present[i] == *previous) {
                selection = static_cast<int>(i + 1);
                languageFilter_ = previous;
                break;
            }
        }
    }
    languageChoice_->SetSelection(selection);
}

void YuGiOhBandaiSetCompletionPanel::onLanguageChoice(wxCommandEvent& /*event*/) {
    const int sel = languageChoice_->GetSelection();
    if (sel <= 0) {
        languageFilter_ = std::nullopt;
    } else {
        const auto present = yuGiOhBandaiLanguagesInCollection(collection_);
        const auto idx = static_cast<std::size_t>(sel - 1);
        if (idx < present.size()) {
            languageFilter_ = present[idx];
        } else {
            languageFilter_ = std::nullopt;
            languageChoice_->SetSelection(0);
        }
    }
    rebuildCurrentView();
}

void YuGiOhBandaiSetCompletionPanel::rebuildCurrentView() {
    if (book_->GetSelection() == 1 && !detailSetId_.empty()) {
        const auto rows =
            computeYuGiOhBandaiSetCompletion(collection_, catalog_, languageFilter_);
        bool stillVisible = false;
        for (const auto& row : rows) {
            if (row.setId == detailSetId_) {
                stillVisible = true;
                break;
            }
        }
        if (!stillVisible) {
            showGridPage();
            rebuildGrid();
            return;
        }
        detailTitle_->SetLabelText(
            wxString::FromUTF8(displaySetName(detailSetName_).c_str()));
        rebuildChecklist(detailSetId_);
    } else {
        rebuildGrid();
    }
}

void YuGiOhBandaiSetCompletionPanel::setEmptyMessage(const wxString& message) {
    clearGridTiles();
    emptyLabel_->SetLabelText(message);
    emptyLabel_->Wrap(480);
    emptyLabel_->Show();
    scroll_->Hide();
    gridPage_->Layout();
}

void YuGiOhBandaiSetCompletionPanel::clearGridTiles() {
    if (gridSizer_ == nullptr) return;
    gridSizer_->Clear(true);
}

void YuGiOhBandaiSetCompletionPanel::rebuildGrid() {
    if (!catalogLoaded_) {
        setEmptyMessage(wxString::FromUTF8(
            "Set checklists are not downloaded yet.\n"
            "Run Sets \xE2\x86\x92 Update Yu-Gi-Oh! (Bandai) to enable Set Completion."));
        return;
    }

    const auto rows = computeYuGiOhBandaiSetCompletion(collection_, catalog_, languageFilter_);
    if (rows.empty()) {
        if (collection_.empty()) {
            setEmptyMessage(wxString::FromUTF8(
                "No Yu-Gi-Oh! (Bandai) sets in progress yet.\n"
                "Add cards on the Single Cards tab to track set completion here."));
        } else {
            bool anyCountable = false;
            for (const auto& card : collection_) {
                if (card.set.id.empty()) continue;
                if (YuGiOhBandaiSetSource::normalizeCardNumber(card.setNo).empty()) continue;
                anyCountable = true;
                break;
            }
            if (!anyCountable) {
                setEmptyMessage(wxString::FromUTF8(
                    "Your cards need a set number (No.) to track set completion.\n"
                    "Edit each card and enter its Bandai number, or use Auto detect."));
            } else {
                setEmptyMessage(wxString::FromUTF8(
                    "None of your cards match a downloaded set checklist.\n"
                    "Run Sets \xE2\x86\x92 Update Yu-Gi-Oh! (Bandai), and confirm "
                    "each card's set and No."));
            }
        }
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

        const std::string title = displaySetName(row.setName);
        auto* nameLbl = new wxStaticText(tile, wxID_ANY, wxString::FromUTF8(title.c_str()));
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

void YuGiOhBandaiSetCompletionPanel::rebuildChecklist(const std::string& setId) {
    checklist_->DeleteAllItems();
    const auto entries =
        yuGiOhBandaiChecklistForSet(collection_, catalog_, setId, languageFilter_);
    const wxColour muted = mutedTextColour(palette_);
    // Fixed green so owned checkmarks stay readable in both light and dark themes.
    const wxColour ownedGreen(46, 160, 67);

    long idx = 0;
    for (const auto& entry : entries) {
        // Align names: checkmark + two spaces vs four spaces for missing cards.
        std::string line =
            (entry.owned ? "\xE2\x9C\x93  " : "    ") + entry.setNo + "  \xE2\x80\x94  " + entry.name;
        if (!entry.rarity.empty()) {
            line += "  (" + entry.rarity + ")";
        }
        const long row = checklist_->InsertItem(idx++, wxString::FromUTF8(line.c_str()));
        if (row < 0) continue;
        if (entry.owned) {
            checklist_->SetItemTextColour(row, ownedGreen);
        } else {
            checklist_->SetItemTextColour(row, muted);
        }
    }
    checklist_->SetColumnWidth(0, wxLIST_AUTOSIZE);
    detailPage_->Layout();
}

}  // namespace ccm::ui
