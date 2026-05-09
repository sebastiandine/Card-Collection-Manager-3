#include "ccm/ui/PokemonCardEditDialog.hpp"

namespace ccm::ui {

PokemonCardEditDialog::PokemonCardEditDialog(wxWindow* parent,
                                             ImageService& imageService,
                                             SetService& setService,
                                             EditMode mode,
                                             PokemonCard initial,
                                             const std::vector<Set>* preloadedSets)
    : BaseCardEditDialog<PokemonCard>(
          parent,
          mode == EditMode::Create ? "Add Pokemon Card" : "Edit Pokemon Card",
          imageService, setService, mode, std::move(initial), Game::Pokemon, preloadedSets) {
    buildAndPopulate();
}

void PokemonCardEditDialog::buildFlagsRow(wxBoxSizer* flagsBox) {
    holoCheck_         = new wxCheckBox(this, wxID_ANY, "Holo");
    firstEditionCheck_ = new wxCheckBox(this, wxID_ANY, "1. Edition");
    signedCheck_       = new wxCheckBox(this, wxID_ANY, "Signed");
    alteredCheck_      = new wxCheckBox(this, wxID_ANY, "Altered");
    flagsBox->Add(holoCheck_,         0, wxRIGHT, 12);
    flagsBox->Add(firstEditionCheck_, 0, wxRIGHT, 12);
    flagsBox->Add(signedCheck_,       0, wxRIGHT, 12);
    flagsBox->Add(alteredCheck_,      0, wxRIGHT, 12);
}

void PokemonCardEditDialog::appendExtraRows(wxFlexGridSizer* grid) {
    setNoCtrl_ = new wxTextCtrl(this, wxID_ANY, constCard().setNo);
    appendRow(grid, "Set #", setNoCtrl_);
}

void PokemonCardEditDialog::readExtraFromCard() {
    if (setNoCtrl_)         setNoCtrl_->ChangeValue(constCard().setNo);
    if (holoCheck_)         holoCheck_->SetValue(constCard().holo);
    if (firstEditionCheck_) firstEditionCheck_->SetValue(constCard().firstEdition);
    if (signedCheck_)       signedCheck_->SetValue(constCard().signed_);
    if (alteredCheck_)      alteredCheck_->SetValue(constCard().altered);
}

void PokemonCardEditDialog::writeExtraToCard() {
    if (setNoCtrl_)         mutableCard().setNo        = setNoCtrl_->GetValue().ToStdString();
    if (holoCheck_)         mutableCard().holo         = holoCheck_->IsChecked();
    if (firstEditionCheck_) mutableCard().firstEdition = firstEditionCheck_->IsChecked();
    if (signedCheck_)       mutableCard().signed_      = signedCheck_->IsChecked();
    if (alteredCheck_)      mutableCard().altered      = alteredCheck_->IsChecked();
}

}  // namespace ccm::ui
