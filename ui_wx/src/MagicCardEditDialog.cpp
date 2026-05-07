#include "ccm/ui/MagicCardEditDialog.hpp"

namespace ccm::ui {

MagicCardEditDialog::MagicCardEditDialog(wxWindow* parent,
                                         ImageService& imageService,
                                         SetService& setService,
                                         EditMode mode,
                                         MagicCard initial,
                                         const std::vector<Set>* preloadedSets)
    : BaseCardEditDialog<MagicCard>(
          parent,
          mode == EditMode::Create ? "Add Magic Card" : "Edit Magic Card",
          imageService, setService, mode, std::move(initial), Game::Magic, preloadedSets) {
    buildAndPopulate();
}

void MagicCardEditDialog::buildFlagsRow(wxBoxSizer* flagsBox) {
    foilCheck_    = new wxCheckBox(this, wxID_ANY, "Foil");
    signedCheck_  = new wxCheckBox(this, wxID_ANY, "Signed");
    alteredCheck_ = new wxCheckBox(this, wxID_ANY, "Altered");
    flagsBox->Add(foilCheck_,    0, wxRIGHT, 12);
    flagsBox->Add(signedCheck_,  0, wxRIGHT, 12);
    flagsBox->Add(alteredCheck_, 0, wxRIGHT, 12);
}

void MagicCardEditDialog::readExtraFromCard() {
    if (foilCheck_)    foilCheck_->SetValue(constCard().foil);
    if (signedCheck_)  signedCheck_->SetValue(constCard().signed_);
    if (alteredCheck_) alteredCheck_->SetValue(constCard().altered);
}

void MagicCardEditDialog::writeExtraToCard() {
    if (foilCheck_)    mutableCard().foil    = foilCheck_->IsChecked();
    if (signedCheck_)  mutableCard().signed_ = signedCheck_->IsChecked();
    if (alteredCheck_) mutableCard().altered = alteredCheck_->IsChecked();
}

}  // namespace ccm::ui
