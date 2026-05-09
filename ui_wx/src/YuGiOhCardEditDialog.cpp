#include "ccm/ui/YuGiOhCardEditDialog.hpp"
#include <wx/panel.h>
#include <cctype>

namespace ccm::ui {
namespace {
const char* const kRarityOptions[] = {
    "Common",
    "Rare",
    "Super Rare",
    "Ultra Rare",
    "Secret Rare",
    "Quarter Century Secret Rare",
    "Starlight Rare",
    "Collector's Rare",
    "Ghost Rare",
    "Ultimate Rare",
    "Platinum Secret Rare",
    "Prismatic Secret Rare",
};
}

YuGiOhCardEditDialog::YuGiOhCardEditDialog(wxWindow* parent,
                                           ImageService& imageService,
                                           SetService& setService,
                                           CardPreviewService& cardPreview,
                                           EditMode mode,
                                           YuGiOhCard initial,
                                           const std::vector<Set>* preloadedSets)
    : BaseCardEditDialog<YuGiOhCard>(
          parent,
          mode == EditMode::Create ? "Add Yu-Gi-Oh! Card" : "Edit Yu-Gi-Oh! Card",
          imageService, setService, mode, std::move(initial), Game::YuGiOh, preloadedSets),
      cardPreview_(cardPreview) {
    buildAndPopulate();
}

void YuGiOhCardEditDialog::buildFlagsRow(wxBoxSizer* flagsBox) {
    firstEditionCheck_ = new wxCheckBox(this, wxID_ANY, "1. Edition");
    signedCheck_       = new wxCheckBox(this, wxID_ANY, "Signed");
    alteredCheck_      = new wxCheckBox(this, wxID_ANY, "Altered");
    flagsBox->Add(firstEditionCheck_, 0, wxRIGHT, 12);
    flagsBox->Add(signedCheck_,       0, wxRIGHT, 12);
    flagsBox->Add(alteredCheck_,      0, wxRIGHT, 12);
}

void YuGiOhCardEditDialog::appendExtraRows(wxFlexGridSizer* grid) {
    auto* setNoPanel = new wxPanel(this, wxID_ANY);
    setNoCtrl_ = new wxTextCtrl(setNoPanel, wxID_ANY);
    setNoCtrl_->Bind(wxEVT_TEXT, &YuGiOhCardEditDialog::onSetNoTextChanged, this);
    autoSetNoBtn_ = new wxButton(setNoPanel, wxID_ANY, "Auto detect");
    autoSetNoBtn_->Bind(wxEVT_BUTTON, &YuGiOhCardEditDialog::onAutoDetectSetNo, this);
    setNoFullPreview_ = new wxStaticText(setNoPanel, wxID_ANY, "(n/a)");
    auto* setNoRow = new wxBoxSizer(wxHORIZONTAL);
    setNoRow->Add(setNoCtrl_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    setNoRow->Add(autoSetNoBtn_, 0, wxALIGN_CENTER_VERTICAL);
    setNoRow->Add(setNoFullPreview_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    setNoPanel->SetSizer(setNoRow);

    auto* rarityPanel = new wxPanel(this, wxID_ANY);
    rarityChoice_ = new wxChoice(rarityPanel, wxID_ANY);
    autoRarityBtn_ = new wxButton(rarityPanel, wxID_ANY, "Auto detect");
    autoRarityBtn_->Bind(wxEVT_BUTTON, &YuGiOhCardEditDialog::onAutoDetectRarity, this);
    wxArrayString rarityItems;
    rarityItems.Alloc(static_cast<int>(sizeof(kRarityOptions) / sizeof(kRarityOptions[0])));
    for (const char* rarity : kRarityOptions) {
        rarityItems.Add(wxString::FromUTF8(rarity));
    }
    rarityChoice_->Append(rarityItems);
    auto* rarityRow = new wxBoxSizer(wxHORIZONTAL);
    rarityRow->Add(rarityChoice_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    rarityRow->Add(autoRarityBtn_, 0, wxALIGN_CENTER_VERTICAL);
    rarityPanel->SetSizer(rarityRow);

    appendRow(grid, "Set #", setNoPanel);
    appendRow(grid, "Rarity", rarityPanel);

    if (auto* setCombo = setComboControl()) {
        setCombo->Bind(wxEVT_COMBOBOX, &YuGiOhCardEditDialog::onSetSelectionChanged, this);
    }
}

void YuGiOhCardEditDialog::readExtraFromCard() {
    if (setNoCtrl_)         setNoCtrl_->ChangeValue(extractSetNoNumeric(constCard().setNo));
    if (rarityChoice_) {
        const wxString rarity = wxString::FromUTF8(constCard().rarity.c_str());
        int rarityIdx = rarityChoice_->FindString(rarity);
        if (rarityIdx == wxNOT_FOUND && !constCard().rarity.empty()) {
            rarityChoice_->Append(rarity);
            rarityIdx = rarityChoice_->GetCount() - 1;
        }
        if (rarityIdx == wxNOT_FOUND) rarityIdx = 0;
        if (rarityIdx != wxNOT_FOUND) rarityChoice_->SetSelection(rarityIdx);
    }
    if (firstEditionCheck_) firstEditionCheck_->SetValue(constCard().firstEdition);
    if (signedCheck_)       signedCheck_->SetValue(constCard().signed_);
    if (alteredCheck_)      alteredCheck_->SetValue(constCard().altered);
    refreshSetNoFullPreview();
}

void YuGiOhCardEditDialog::writeExtraToCard() {
    if (setNoCtrl_)         mutableCard().setNo        = composeFullSetNo(setNoCtrl_->GetValue().ToStdString());
    if (rarityChoice_)      mutableCard().rarity       = rarityChoice_->GetStringSelection().ToStdString();
    if (firstEditionCheck_) mutableCard().firstEdition = firstEditionCheck_->IsChecked();
    if (signedCheck_)       mutableCard().signed_      = signedCheck_->IsChecked();
    if (alteredCheck_)      mutableCard().altered      = alteredCheck_->IsChecked();
}

void YuGiOhCardEditDialog::onAutoDetectSetNo(wxCommandEvent&) {
    autoDetectFromApi(true, false);
}

void YuGiOhCardEditDialog::onAutoDetectRarity(wxCommandEvent&) {
    autoDetectFromApi(false, true);
}

void YuGiOhCardEditDialog::autoDetectFromApi(bool fillSetNo, bool fillRarity) {
    syncCardFromControls();
    const auto& card = constCard();
    if (card.name.empty()) {
        showThemedMessageDialog(this, "Enter a card name first.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return;
    }
    if (card.set.name.empty()) {
        showThemedMessageDialog(this, "Select a set first.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return;
    }

    auto detected = cardPreview_.detectFirstPrint(Game::YuGiOh, card.name, card.set.name);
    if (!detected) {
        showThemedMessageDialog(this, "Auto detect failed: " + detected.error(), "Auto detect",
                                wxOK | wxICON_WARNING);
        return;
    }

    if (fillSetNo && setNoCtrl_) {
        setNoCtrl_->ChangeValue(wxString::FromUTF8(extractSetNoNumeric(detected.value().setNo).c_str()));
    }
    if (fillRarity && rarityChoice_) {
        const wxString rarity = wxString::FromUTF8(detected.value().rarity.c_str());
        int idx = rarityChoice_->FindString(rarity);
        if (idx == wxNOT_FOUND && !detected.value().rarity.empty()) {
            rarityChoice_->Append(rarity);
            idx = rarityChoice_->GetCount() - 1;
        }
        if (idx != wxNOT_FOUND) rarityChoice_->SetSelection(idx);
    }
    refreshSetNoFullPreview();
}

void YuGiOhCardEditDialog::onSetNoTextChanged(wxCommandEvent&) {
    refreshSetNoFullPreview();
}

void YuGiOhCardEditDialog::onSetSelectionChanged(wxCommandEvent& ev) {
    refreshSetNoFullPreview();
    ev.Skip();
}

std::string YuGiOhCardEditDialog::extractSetNoNumeric(std::string_view fullSetNo) const {
    std::string digits;
    for (char ch : fullSetNo) {
        if (std::isdigit(static_cast<unsigned char>(ch))) digits.push_back(ch);
    }
    return digits;
}

std::string YuGiOhCardEditDialog::composeFullSetNo(std::string_view numericRaw) const {
    std::string numeric;
    for (char ch : numericRaw) {
        if (std::isdigit(static_cast<unsigned char>(ch))) numeric.push_back(ch);
    }
    const Set* set = selectedSetFromControls();
    if (!set || set->id.empty() || numeric.empty()) return numeric;
    return set->id + "-" + numeric;
}

void YuGiOhCardEditDialog::refreshSetNoFullPreview() {
    if (!setNoFullPreview_ || !setNoCtrl_) return;
    const std::string full = composeFullSetNo(setNoCtrl_->GetValue().ToStdString());
    if (full.empty()) {
        setNoFullPreview_->SetLabel("(n/a)");
    } else {
        setNoFullPreview_->SetLabel(wxString::Format("(%s)", wxString::FromUTF8(full.c_str())));
    }
    setNoFullPreview_->GetParent()->Layout();
}

}  // namespace ccm::ui
