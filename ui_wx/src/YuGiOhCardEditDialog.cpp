#include "ccm/ui/YuGiOhCardEditDialog.hpp"

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
                                           EditMode mode,
                                           YuGiOhCard initial,
                                           const std::vector<Set>* preloadedSets)
    : BaseCardEditDialog<YuGiOhCard>(
          parent,
          mode == EditMode::Create ? "Add Yu-Gi-Oh! Card" : "Edit Yu-Gi-Oh! Card",
          imageService, setService, mode, std::move(initial), Game::YuGiOh, preloadedSets) {
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
    setNoCtrl_ = new wxTextCtrl(this, wxID_ANY, constCard().setNo);
    rarityChoice_ = new wxChoice(this, wxID_ANY);
    wxArrayString rarityItems;
    rarityItems.Alloc(static_cast<int>(sizeof(kRarityOptions) / sizeof(kRarityOptions[0])));
    for (const char* rarity : kRarityOptions) {
        rarityItems.Add(wxString::FromUTF8(rarity));
    }
    rarityChoice_->Append(rarityItems);
    appendRow(grid, "Set #", setNoCtrl_);
    appendRow(grid, "Rarity", rarityChoice_);
}

void YuGiOhCardEditDialog::readExtraFromCard() {
    if (setNoCtrl_)         setNoCtrl_->ChangeValue(constCard().setNo);
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
}

void YuGiOhCardEditDialog::writeExtraToCard() {
    if (setNoCtrl_)         mutableCard().setNo        = setNoCtrl_->GetValue().ToStdString();
    if (rarityChoice_)      mutableCard().rarity       = rarityChoice_->GetStringSelection().ToStdString();
    if (firstEditionCheck_) mutableCard().firstEdition = firstEditionCheck_->IsChecked();
    if (signedCheck_)       mutableCard().signed_      = signedCheck_->IsChecked();
    if (alteredCheck_)      mutableCard().altered      = alteredCheck_->IsChecked();
}

}  // namespace ccm::ui
