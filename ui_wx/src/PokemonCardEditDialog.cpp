#include "ccm/ui/PokemonCardEditDialog.hpp"

#include "ccm/domain/Enums.hpp"
#include <wx/app.h>
#include <wx/panel.h>
#include <thread>
#include <unordered_set>

namespace ccm::ui {

PokemonCardEditDialog::PokemonCardEditDialog(wxWindow* parent,
                                             ImageService& imageService,
                                             SetService& setService,
                                             CardPreviewService& cardPreview,
                                             EditMode mode,
                                             PokemonCard initial,
                                             const std::vector<Set>* preloadedSets)
    : BaseCardEditDialog<PokemonCard>(
          parent,
          mode == EditMode::Create ? "Add Pokemon Card" : "Edit Pokemon Card",
          imageService, setService, mode, std::move(initial), Game::Pokemon, preloadedSets),
      dialogMode_(mode),
      cardPreview_(cardPreview),
      variantFetchState_(std::make_shared<VariantFetchState>()) {
    buildAndPopulate();
    if (dialogMode_ == EditMode::Edit) {
        scheduleDeferredVariantPrefetch();
    }
}

PokemonCardEditDialog::~PokemonCardEditDialog() {
    if (variantFetchState_) {
        variantFetchState_->alive.store(false);
    }
}

void PokemonCardEditDialog::onCardLookupContextChanged() {
    clearCachedPrintVariants();
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
    auto* setNoPanel = new wxPanel(this, wxID_ANY);
    setNoCtrl_ = new wxTextCtrl(setNoPanel, wxID_ANY);
    autoSetNoBtn_ = new wxButton(setNoPanel, wxID_ANY, "Auto detect");
    autoSetNoBtn_->Bind(wxEVT_BUTTON, &PokemonCardEditDialog::onAutoDetectSetNo, this);
    nextSetNoBtn_ = new wxButton(setNoPanel, wxID_ANY, "Next");
    nextSetNoBtn_->Bind(wxEVT_BUTTON, &PokemonCardEditDialog::onNextSetNo, this);
    nextSetNoBtn_->Show(false);
    auto* setNoRow = new wxBoxSizer(wxHORIZONTAL);
    setNoRow->Add(setNoCtrl_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    setNoRow->Add(autoSetNoBtn_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    setNoRow->Add(nextSetNoBtn_, 0, wxALIGN_CENTER_VERTICAL);
    setNoPanel->SetSizer(setNoRow);

    appendRow(grid, "Set #", setNoPanel);

    if (auto* setCombo = setComboControl()) {
        setCombo->Bind(wxEVT_COMBOBOX, &PokemonCardEditDialog::onSetSelectionChanged, this);
    }
}

std::string PokemonCardEditDialog::normalizedStoredSetNo(std::string_view setNo) {
    std::string out(setNo);
    const auto slash = out.find('/');
    if (slash != std::string::npos) {
        out.resize(slash);
    }
    return out;
}

std::string PokemonCardEditDialog::storedSetNoFromControls(const wxTextCtrl* ctrl) {
    if (ctrl == nullptr) return {};
    return normalizedStoredSetNo(ctrl->GetValue().ToStdString(wxConvUTF8));
}

void PokemonCardEditDialog::readExtraFromCard() {
    clearCachedPrintVariants();
    if (setNoCtrl_) {
        setNoCtrl_->ChangeValue(
            wxString::FromUTF8(normalizedStoredSetNo(constCard().setNo).c_str()));
    }
    if (holoCheck_)         holoCheck_->SetValue(constCard().holo);
    if (firstEditionCheck_) firstEditionCheck_->SetValue(constCard().firstEdition);
    if (signedCheck_)       signedCheck_->SetValue(constCard().signed_);
    if (alteredCheck_)      alteredCheck_->SetValue(constCard().altered);
}

void PokemonCardEditDialog::writeExtraToCard() {
    if (setNoCtrl_)         mutableCard().setNo        = storedSetNoFromControls(setNoCtrl_);
    if (holoCheck_)         mutableCard().holo         = holoCheck_->IsChecked();
    if (firstEditionCheck_) mutableCard().firstEdition = firstEditionCheck_->IsChecked();
    if (signedCheck_)       mutableCard().signed_      = signedCheck_->IsChecked();
    if (alteredCheck_)      mutableCard().altered      = alteredCheck_->IsChecked();
}

void PokemonCardEditDialog::clearCachedPrintVariants() {
    ++variantFetchEpoch_;
    cachedVariants_.clear();
    uniqueSetNos_.clear();
    setNoRingPos_ = 0;
    refreshVariantNextControls();
}

void PokemonCardEditDialog::scheduleDeferredVariantPrefetch() {
    const unsigned epoch = variantFetchEpoch_;
    wxTheApp->CallAfter([this, epoch]() {
        prefetchVariantsForCurrentCardSilent(epoch);
    });
}

void PokemonCardEditDialog::prefetchVariantsForCurrentCardSilent(unsigned capturedEpoch) {
    if (capturedEpoch != variantFetchEpoch_) return;
    if (!cachedVariants_.empty()) return;
    const auto& card = constCard();
    if (card.name.empty() || card.set.id.empty()) return;

    requestVariantsAsync(capturedEpoch, card.name, card.set.id, false, false);
}

void PokemonCardEditDialog::requestVariantsAsync(unsigned capturedEpoch,
                                                 std::string name,
                                                 std::string setId,
                                                 bool fillSetNoOnSuccess,
                                                 bool showFailureDialog) {
    if (capturedEpoch != variantFetchEpoch_) return;

    if (fillSetNoOnSuccess && autoSetNoBtn_) {
        autoSetNoBtn_->Disable();
    }

    auto state = variantFetchState_;
    CardPreviewService* svc = &cardPreview_;
    PokemonCardEditDialog* self = this;
    std::thread([state, svc, self, capturedEpoch, name = std::move(name),
                 setId = std::move(setId), fillSetNoOnSuccess, showFailureDialog]() {
        auto detected = svc->detectPrintVariants(Game::Pokemon, name, setId);
        wxTheApp->CallAfter([state, self, capturedEpoch, detected = std::move(detected),
                             fillSetNoOnSuccess, showFailureDialog]() mutable {
            if (!state->alive.load()) return;
            self->applyDetectedVariants(capturedEpoch, std::move(detected),
                                        fillSetNoOnSuccess, showFailureDialog);
        });
    }).detach();
}

void PokemonCardEditDialog::applyDetectedVariants(unsigned capturedEpoch,
                                                  Result<std::vector<AutoDetectedPrint>> detected,
                                                  bool fillSetNoOnSuccess,
                                                  bool showFailureDialog) {
    if (capturedEpoch != variantFetchEpoch_) return;

    if (fillSetNoOnSuccess && autoSetNoBtn_) {
        autoSetNoBtn_->Enable();
    }

    if (!detected) {
        if (showFailureDialog) {
            showThemedMessageDialog(this, "Auto detect failed: " + detected.error(), "Auto detect",
                                    wxOK | wxICON_WARNING);
        }
        return;
    }

    cachedVariants_ = std::move(detected).value();
    if (fillSetNoOnSuccess && setNoCtrl_ && !cachedVariants_.empty()) {
        setNoCtrl_->ChangeValue(
            wxString::FromUTF8(cachedVariants_.front().setNo.c_str()));
    }

    rebuildVariantRingFromCache();
    syncRingPositionToControls();
    refreshVariantNextControls();
}

void PokemonCardEditDialog::rebuildVariantRingFromCache() {
    uniqueSetNos_.clear();
    if (cachedVariants_.empty()) return;

    std::unordered_set<std::string> seen;
    seen.reserve(cachedVariants_.size());
    for (const auto& p : cachedVariants_) {
        if (p.setNo.empty()) continue;
        if (!seen.insert(p.setNo).second) continue;
        uniqueSetNos_.push_back(p.setNo);
    }
}

void PokemonCardEditDialog::syncRingPositionToControls() {
    if (!setNoCtrl_) return;
    const std::string current = storedSetNoFromControls(setNoCtrl_);
    setNoRingPos_ = 0;
    for (std::size_t i = 0; i < uniqueSetNos_.size(); ++i) {
        if (uniqueSetNos_[i] == current) {
            setNoRingPos_ = i;
            break;
        }
    }
}

void PokemonCardEditDialog::refreshVariantNextControls() {
    if (!nextSetNoBtn_) return;
    nextSetNoBtn_->Show(uniqueSetNos_.size() > 1);
    Layout();
    if (GetSizer()) Fit();
}

void PokemonCardEditDialog::onAutoDetectSetNo(wxCommandEvent&) {
    autoDetectFromApi();
}

void PokemonCardEditDialog::onNextSetNo(wxCommandEvent&) {
    if (uniqueSetNos_.size() <= 1) return;
    setNoRingPos_ = (setNoRingPos_ + 1) % uniqueSetNos_.size();
    if (setNoCtrl_) {
        setNoCtrl_->ChangeValue(wxString::FromUTF8(uniqueSetNos_[setNoRingPos_].c_str()));
    }
    refreshVariantNextControls();
}

void PokemonCardEditDialog::autoDetectFromApi() {
    syncCardFromControls();
    const auto& card = constCard();
    if (card.name.empty()) {
        showThemedMessageDialog(this, "Enter a card name first.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return;
    }
    if (card.set.id.empty()) {
        showThemedMessageDialog(this, "Select a set first.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return;
    }

    const unsigned epoch = variantFetchEpoch_;
    requestVariantsAsync(epoch, card.name, card.set.id, true, true);
}

void PokemonCardEditDialog::onSetSelectionChanged(wxCommandEvent& ev) {
    clearCachedPrintVariants();
    scheduleDeferredVariantPrefetch();
    ev.Skip();
}

}  // namespace ccm::ui
