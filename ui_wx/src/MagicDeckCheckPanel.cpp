#include "ccm/ui/MagicDeckCheckPanel.hpp"

#include "ccm/domain/Enums.hpp"
#include "ccm/ui/SvgIcons.hpp"

#include <wx/button.h>
#include <wx/cursor.h>
#include <wx/scrolwin.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <algorithm>
#include <string>

namespace ccm::ui {

namespace {

wxColour mutedTextColour(const ThemePalette& palette) {
    const auto blend = [](unsigned char a, unsigned char b) -> unsigned char {
        return static_cast<unsigned char>((static_cast<int>(a) * 2 + static_cast<int>(b)) / 3);
    };
    return wxColour(blend(palette.text.Red(), palette.panelBg.Red()),
                    blend(palette.text.Green(), palette.panelBg.Green()),
                    blend(palette.text.Blue(), palette.panelBg.Blue()));
}

constexpr const char kPasteHint[] = "4 Lightning Bolt";
constexpr int kStatusIconPx = 16;
constexpr const char kStatusRed[] = "#E53935";
constexpr const char kStatusYellow[] = "#F9A825";
constexpr const char kStatusGreen[] = "#2EA043";

}  // namespace

MagicDeckCheckPanel::MagicDeckCheckPanel(wxWindow* parent) : wxPanel(parent) {
    palette_ = paletteForTheme(inferThemeFromWindow(this));

    book_ = new wxSimplebook(this, wxID_ANY);

    inputPage_ = new wxPanel(book_);
    auto* inputSizer = new wxBoxSizer(wxVERTICAL);
    inputHint_ = new wxStaticText(
        inputPage_, wxID_ANY,
        wxString::FromUTF8("Paste a deck list (one card per line, quantity first)."));
    pasteInput_ = new wxTextCtrl(inputPage_, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                 wxTE_MULTILINE | wxTE_RICH2 | wxTE_DONTWRAP);
    installTextCtrlPlaceholder(pasteInput_, wxString::FromUTF8(kPasteHint));
    analyzeBtn_ = new wxButton(inputPage_, wxID_ANY, wxString::FromUTF8("Analyze"));
    analyzeBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onAnalyze(); });
    inputSizer->Add(inputHint_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);
    inputSizer->Add(pasteInput_, 1, wxEXPAND | wxALL, 12);
    inputSizer->Add(analyzeBtn_, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    inputPage_->SetSizer(inputSizer);
    book_->AddPage(inputPage_, "Input");

    resultPage_ = new wxPanel(book_);
    auto* resultRoot = new wxBoxSizer(wxVERTICAL);
    auto* topRow = new wxBoxSizer(wxHORIZONTAL);
    clearBtn_ = new wxButton(resultPage_, wxID_ANY, wxString::FromUTF8("Clear"));
    clearBtn_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { onClear(); });
    topRow->Add(clearBtn_, 0, wxALIGN_CENTER_VERTICAL);
    topRow->AddStretchSpacer(1);
    resultRoot->Add(topRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 12);

    scroll_ = new wxScrolledWindow(resultPage_, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxVSCROLL | wxTAB_TRAVERSAL);
    scroll_->SetScrollRate(0, 16);
    resultSizer_ = new wxBoxSizer(wxVERTICAL);
    scroll_->SetSizer(resultSizer_);
    resultRoot->Add(scroll_, 1, wxEXPAND | wxALL, 8);
    resultPage_->SetSizer(resultRoot);
    book_->AddPage(resultPage_, "Result");

    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(book_, 1, wxEXPAND);
    SetSizer(root);

    showInputPage();
}

void MagicDeckCheckPanel::setCollection(std::vector<MagicCard> cards) {
    collection_ = std::move(cards);
    refreshAnalysisIfActive();
}

void MagicDeckCheckPanel::applyTheme(const ThemePalette& palette) {
    palette_ = palette;
    const Theme theme = inferThemeFromWindow(this);
    applyThemeToWindowTree(this, palette, theme);
    applyPaletteToTextCtrl(pasteInput_, palette, theme);
    if (showingResult_) rebuildResults();
}

void MagicDeckCheckPanel::onAnalyze() {
    const std::string text = pasteInput_->GetValue().ToStdString(wxConvUTF8);
    const auto deck = parseMagicDeckList(text);
    if (deck.empty()) {
        showThemedMessageDialog(this,
                                wxString::FromUTF8("No numbered card lines found."),
                                wxString::FromUTF8("Deck Check"), wxOK | wxICON_INFORMATION);
        return;
    }
    result_ = analyzeMagicDeckCheck(deck, collection_);
    showingResult_ = true;
    rebuildResults();
    showResultPage();
}

void MagicDeckCheckPanel::onClear() {
    showingResult_ = false;
    result_ = {};
    if (resultSizer_ != nullptr) resultSizer_->Clear(true);
    showInputPage();
}

void MagicDeckCheckPanel::showInputPage() {
    if (book_ != nullptr) book_->SetSelection(0);
}

void MagicDeckCheckPanel::showResultPage() {
    if (book_ != nullptr) book_->SetSelection(1);
}

void MagicDeckCheckPanel::refreshAnalysisIfActive() {
    if (!showingResult_ || pasteInput_ == nullptr) return;
    const std::string text = pasteInput_->GetValue().ToStdString(wxConvUTF8);
    result_ = analyzeMagicDeckCheck(parseMagicDeckList(text), collection_);
    rebuildResults();
}

void MagicDeckCheckPanel::rebuildResults() {
    if (scroll_ == nullptr || resultSizer_ == nullptr) return;

    scroll_->Freeze();
    resultSizer_->Clear(true);

    auto addSectionTitle = [&](const char* title) {
        auto* label = new wxStaticText(scroll_, wxID_ANY, wxString::FromUTF8(title));
        auto font = label->GetFont();
        font.MakeBold().MakeLarger();
        label->SetFont(font);
        label->SetForegroundColour(palette_.text);
        resultSizer_->Add(label, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    };

    addSectionTitle("Missing");
    if (result_.missing.empty()) {
        auto* empty = new wxStaticText(scroll_, wxID_ANY, wxString::FromUTF8("None missing"));
        empty->SetForegroundColour(mutedTextColour(palette_));
        resultSizer_->Add(empty, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    } else {
        auto missing = result_.missing;
        std::stable_sort(missing.begin(), missing.end(),
                         [](const MagicDeckMissingRow& a, const MagicDeckMissingRow& b) {
                             const bool aComplete = a.missingAmount == a.required;
                             const bool bComplete = b.missingAmount == b.required;
                             return aComplete && !bComplete;
                         });
        for (const auto& row : missing) addMissingRow(row);
    }

    addSectionTitle("Owned");
    if (result_.owned.empty()) {
        auto* empty = new wxStaticText(scroll_, wxID_ANY, wxString::FromUTF8("None owned"));
        empty->SetForegroundColour(mutedTextColour(palette_));
        resultSizer_->Add(empty, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    } else {
        auto owned = result_.owned;
        std::stable_sort(owned.begin(), owned.end(),
                         [](const MagicDeckOwnedRow& a, const MagicDeckOwnedRow& b) {
                             const bool aShort = a.ownedAmount < a.required;
                             const bool bShort = b.ownedAmount < b.required;
                             return aShort && !bShort;
                         });
        for (const auto& row : owned) addOwnedRow(row);
    }

    resultSizer_->AddStretchSpacer(1);
    scroll_->FitInside();
    resultPage_->Layout();
    Layout();
    scroll_->Thaw();
}

void MagicDeckCheckPanel::addStatusIcon(wxWindow* parent, wxBoxSizer* sizer, const char* svg,
                                        const char* fillHex) {
    auto* icon = new wxStaticBitmap(parent, wxID_ANY, svgIconBitmap(svg, kStatusIconPx, fillHex));
    sizer->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
}

void MagicDeckCheckPanel::addMissingRow(const MagicDeckMissingRow& row) {
    auto* wrap = new wxPanel(scroll_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
    wrap->SetBackgroundColour(palette_.buttonBg);
    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->AddSpacer(8);
    const bool completeMiss = row.missingAmount >= row.required;
    addStatusIcon(wrap, sizer, kSvgAlert, completeMiss ? kStatusRed : kStatusYellow);
    const std::string text = row.name + " — " + std::to_string(row.missingAmount) + " of " +
                             std::to_string(row.required) + " still needed";
    auto* label = new wxStaticText(wrap, wxID_ANY, wxString::FromUTF8(text.c_str()));
    label->SetForegroundColour(palette_.text);
    sizer->Add(label, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 8);
    wrap->SetSizer(sizer);
    resultSizer_->Add(wrap, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
}

void MagicDeckCheckPanel::addOwnedRow(const MagicDeckOwnedRow& row) {
    auto* wrap = new wxPanel(scroll_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
    wrap->SetBackgroundColour(palette_.buttonBg);
    auto* wrapSizer = new wxBoxSizer(wxVERTICAL);

    auto* header = new wxPanel(wrap);
    header->SetBackgroundColour(palette_.buttonBg);
    auto* headerSizer = new wxBoxSizer(wxHORIZONTAL);

    const bool stillShort = row.ownedAmount < row.required;
    if (stillShort) {
        addStatusIcon(header, headerSizer, kSvgAlert, kStatusYellow);
    } else {
        addStatusIcon(header, headerSizer, kSvgCheck, kStatusGreen);
    }

    const bool expandable = row.variations.size() >= 2;
    wxStaticText* chevron = nullptr;
    if (expandable) {
        chevron = new wxStaticText(header, wxID_ANY, wxString::FromUTF8("\xE2\x96\xB8"));
        chevron->SetForegroundColour(palette_.text);
        headerSizer->Add(chevron, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
        header->SetCursor(wxCursor(wxCURSOR_HAND));
        chevron->SetCursor(wxCursor(wxCURSOR_HAND));
    }

    const std::string title = row.name + " — " + std::to_string(row.ownedAmount) + " owned (need " +
                              std::to_string(row.required) + ")";
    auto* titleLbl = new wxStaticText(header, wxID_ANY, wxString::FromUTF8(title.c_str()));
    titleLbl->SetForegroundColour(palette_.text);
    headerSizer->Add(titleLbl, 1, wxALIGN_CENTER_VERTICAL | wxEXPAND);

    if (!expandable && !row.variations.empty()) {
        auto* detail = new wxStaticText(header, wxID_ANY, variationLabel(row.variations.front()));
        detail->SetForegroundColour(mutedTextColour(palette_));
        headerSizer->Add(detail, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 12);
    }

    header->SetSizer(headerSizer);
    wrapSizer->Add(header, 0, wxEXPAND | wxALL, 8);

    if (expandable) {
        auto* details = new wxPanel(wrap);
        details->SetBackgroundColour(palette_.buttonBg);
        auto* detailSizer = new wxBoxSizer(wxVERTICAL);
        for (const auto& card : row.variations) {
            auto* line = new wxStaticText(details, wxID_ANY, variationLabel(card));
            line->SetForegroundColour(mutedTextColour(palette_));
            detailSizer->Add(line, 0, wxEXPAND | wxBOTTOM, 4);
        }
        details->SetSizer(detailSizer);
        details->Hide();
        wrapSizer->Add(details, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

        auto toggle = [this, details, chevron, wrap](wxMouseEvent&) {
            const bool show = !details->IsShown();
            details->Show(show);
            if (chevron != nullptr) {
                chevron->SetLabelText(
                    wxString::FromUTF8(show ? "\xE2\x96\xBE" : "\xE2\x96\xB8"));
            }
            wrap->Layout();
            scroll_->FitInside();
            scroll_->Layout();
            resultPage_->Layout();
        };
        header->Bind(wxEVT_LEFT_UP, toggle);
        titleLbl->Bind(wxEVT_LEFT_UP, toggle);
        chevron->Bind(wxEVT_LEFT_UP, toggle);
        titleLbl->SetCursor(wxCursor(wxCURSOR_HAND));
    }

    wrap->SetSizer(wrapSizer);
    resultSizer_->Add(wrap, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 6);
}

wxString MagicDeckCheckPanel::variationLabel(const MagicCard& card) const {
    std::string text = card.set.name;
    if (text.empty()) text = card.set.id;
    text += " \xC2\xB7 ";
    text += std::string(to_string(card.language));
    text += " \xC2\xB7 ";
    text += std::string(to_string(card.condition));
    text += " \xC2\xB7 ";
    text += std::to_string(card.amount);
    if (card.foil) text += " \xC2\xB7 Foil";
    return wxString::FromUTF8(text.c_str());
}

}  // namespace ccm::ui
