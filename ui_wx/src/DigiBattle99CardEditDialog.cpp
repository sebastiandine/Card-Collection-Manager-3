#include "ccm/ui/DigiBattle99CardEditDialog.hpp"

#include "ccm/domain/Enums.hpp"
#include "ccm/games/digibattle99/DigiBattle99CardPreviewSource.hpp"
#include <wx/app.h>
#include <wx/panel.h>
#include <cctype>
#include <thread>
#include <unordered_set>

namespace ccm::ui {

DigiBattle99CardEditDialog::DigiBattle99CardEditDialog(wxWindow* parent,
                                                       ImageService& imageService,
                                                       SetService& setService,
                                                       CardPreviewService& cardPreview,
                                                       EditMode mode,
                                                       DigiBattle99Card initial,
                                                       const std::vector<Set>* preloadedSets)
    : BaseCardEditDialog<DigiBattle99Card>(
          parent,
          mode == EditMode::Create ? "Add Digimon (Digi-Battle) Card"
                                   : "Edit Digimon (Digi-Battle) Card",
          imageService, setService, mode, std::move(initial), Game::DigiBattle99,
          preloadedSets),
      dialogMode_(mode),
      cardPreview_(cardPreview),
      variantFetchState_(std::make_shared<VariantFetchState>()) {
    buildAndPopulate();
    if (dialogMode_ == EditMode::Edit) {
        scheduleDeferredVariantPrefetch();
    }
}

DigiBattle99CardEditDialog::~DigiBattle99CardEditDialog() {
    if (variantFetchState_) {
        variantFetchState_->alive.store(false);
    }
}

void DigiBattle99CardEditDialog::onCardLookupContextChanged() {
    clearCachedPrintVariants();
}

void DigiBattle99CardEditDialog::buildFlagsRow(wxBoxSizer* flagsBox) {
    holoCheck_         = new wxCheckBox(this, wxID_ANY, "Holo");
    firstEditionCheck_ = new wxCheckBox(this, wxID_ANY, "1. Edition");
    signedCheck_       = new wxCheckBox(this, wxID_ANY, "Signed");
    alteredCheck_      = new wxCheckBox(this, wxID_ANY, "Altered");
    flagsBox->Add(holoCheck_,         0, wxRIGHT, 12);
    flagsBox->Add(firstEditionCheck_, 0, wxRIGHT, 12);
    flagsBox->Add(signedCheck_,       0, wxRIGHT, 12);
    flagsBox->Add(alteredCheck_,      0, wxRIGHT, 12);
}

void DigiBattle99CardEditDialog::appendExtraRows(wxFlexGridSizer* grid) {
    auto* setNoPanel = new wxPanel(this, wxID_ANY);
    setNoCtrl_ = new wxTextCtrl(setNoPanel, wxID_ANY);
    autoSetNoBtn_ = new wxButton(setNoPanel, wxID_ANY, "Auto detect");
    autoSetNoBtn_->Bind(wxEVT_BUTTON, &DigiBattle99CardEditDialog::onAutoDetectSetNo, this);
    nextSetNoBtn_ = new wxButton(setNoPanel, wxID_ANY, "Next");
    nextSetNoBtn_->Bind(wxEVT_BUTTON, &DigiBattle99CardEditDialog::onNextSetNo, this);
    nextSetNoBtn_->Show(false);
    setNoCtrl_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { markSetNoLookupEdited(); });
    auto* setNoRow = new wxBoxSizer(wxHORIZONTAL);
    setNoRow->Add(setNoCtrl_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    setNoRow->Add(autoSetNoBtn_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    setNoRow->Add(nextSetNoBtn_, 0, wxALIGN_CENTER_VERTICAL);
    setNoPanel->SetSizer(setNoRow);

    appendRow(grid, "Set #", setNoPanel);

    if (auto* setCombo = setComboControl()) {
        setCombo->Bind(wxEVT_COMBOBOX, &DigiBattle99CardEditDialog::onSetSelectionChanged, this);
    }
}

std::string DigiBattle99CardEditDialog::normalizedStoredSetNo(std::string_view setNo) {
    return DigiBattle99CardPreviewSource::normalizeCardNumber(setNo);
}

std::string DigiBattle99CardEditDialog::storedSetNoFromControls(const wxTextCtrl* ctrl) {
    if (ctrl == nullptr) return {};
    return normalizedStoredSetNo(ctrl->GetValue().ToStdString(wxConvUTF8));
}

void DigiBattle99CardEditDialog::readExtraFromCard() {
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

void DigiBattle99CardEditDialog::writeExtraToCard() {
    if (setNoCtrl_)         mutableCard().setNo        = storedSetNoFromControls(setNoCtrl_);
    if (holoCheck_)         mutableCard().holo         = holoCheck_->IsChecked();
    if (firstEditionCheck_) mutableCard().firstEdition = firstEditionCheck_->IsChecked();
    if (signedCheck_)       mutableCard().signed_      = signedCheck_->IsChecked();
    if (alteredCheck_)      mutableCard().altered      = alteredCheck_->IsChecked();
}

void DigiBattle99CardEditDialog::clearCachedPrintVariants() {
    ++variantFetchEpoch_;
    cachedVariants_.clear();
    uniqueSetNos_.clear();
    setNoRingPos_ = 0;
    refreshVariantNextControls();
}

void DigiBattle99CardEditDialog::scheduleDeferredVariantPrefetch() {
    const unsigned epoch = variantFetchEpoch_;
    wxTheApp->CallAfter([this, epoch]() {
        prefetchVariantsForCurrentCardSilent(epoch);
    });
}

void DigiBattle99CardEditDialog::prefetchVariantsForCurrentCardSilent(unsigned capturedEpoch) {
    if (capturedEpoch != variantFetchEpoch_) return;
    if (!cachedVariants_.empty()) return;
    const auto& card = constCard();
    // digimoncard.io pack= uses the display set name, not the slug id.
    if (card.name.empty() || card.set.name.empty()) return;

    requestVariantsAsync(capturedEpoch, card.name, card.set.name, false, false);
}

void DigiBattle99CardEditDialog::requestVariantsAsync(unsigned capturedEpoch,
                                                      std::string name,
                                                      std::string setName,
                                                      bool fillSetNoOnSuccess,
                                                      bool showFailureDialog) {
    if (capturedEpoch != variantFetchEpoch_) return;

    if (fillSetNoOnSuccess && autoSetNoBtn_) {
        autoSetNoBtn_->Disable();
    }

    auto state = variantFetchState_;
    CardPreviewService* svc = &cardPreview_;
    DigiBattle99CardEditDialog* self = this;
    std::thread([state, svc, self, capturedEpoch, name = std::move(name),
                 setName = std::move(setName), fillSetNoOnSuccess, showFailureDialog]() {
        auto detected = svc->detectPrintVariants(Game::DigiBattle99, name, setName);
        wxTheApp->CallAfter([state, self, capturedEpoch, detected = std::move(detected),
                             fillSetNoOnSuccess, showFailureDialog]() mutable {
            if (!state->alive.load()) return;
            self->applyDetectedVariants(capturedEpoch, std::move(detected),
                                        fillSetNoOnSuccess, /*fillNameOnSuccess=*/false,
                                        showFailureDialog);
        });
    }).detach();
}

void DigiBattle99CardEditDialog::requestBySetNoAsync(unsigned capturedEpoch,
                                                     std::string setName,
                                                     std::string setNo,
                                                     bool showFailureDialog) {
    if (capturedEpoch != variantFetchEpoch_) return;
    if (showFailureDialog && autoSetNoBtn_) autoSetNoBtn_->Disable();

    auto state = variantFetchState_;
    CardPreviewService* svc = &cardPreview_;
    DigiBattle99CardEditDialog* self = this;
    std::thread([state, svc, self, capturedEpoch, setName = std::move(setName),
                 setNo = std::move(setNo), showFailureDialog]() {
        auto detected = svc->detectVariantsBySetNo(Game::DigiBattle99, setName, setNo);
        wxTheApp->CallAfter([state, self, capturedEpoch, detected = std::move(detected),
                             showFailureDialog]() mutable {
            if (!state->alive.load()) return;
            self->applyDetectedVariants(capturedEpoch, std::move(detected),
                                        /*fillSetNoOnSuccess=*/true,
                                        /*fillNameOnSuccess=*/true, showFailureDialog);
        });
    }).detach();
}

void DigiBattle99CardEditDialog::applyDetectedVariants(
    unsigned capturedEpoch,
    Result<std::vector<AutoDetectedPrint>> detected,
    bool fillSetNoOnSuccess,
    bool fillNameOnSuccess,
    bool showFailureDialog) {
    if (capturedEpoch != variantFetchEpoch_) return;

    if ((fillSetNoOnSuccess || fillNameOnSuccess) && autoSetNoBtn_) {
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
    if (!cachedVariants_.empty()) {
        const auto& first = cachedVariants_.front();
        if (fillNameOnSuccess && !first.name.empty()) {
            if (auto* name = nameControl()) {
                name->ChangeValue(wxString::FromUTF8(first.name.c_str()));
            }
        }
        if (fillSetNoOnSuccess && setNoCtrl_) {
            setNoCtrl_->ChangeValue(wxString::FromUTF8(first.setNo.c_str()));
        }
    }

    rebuildVariantRingFromCache();
    syncRingPositionToControls();
    refreshVariantNextControls();
}

void DigiBattle99CardEditDialog::rebuildVariantRingFromCache() {
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

void DigiBattle99CardEditDialog::syncRingPositionToControls() {
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

void DigiBattle99CardEditDialog::refreshVariantNextControls() {
    if (!nextSetNoBtn_) return;
    nextSetNoBtn_->Show(uniqueSetNos_.size() > 1);
    Layout();
    if (GetSizer()) Fit();
}

void DigiBattle99CardEditDialog::onAutoDetectSetNo(wxCommandEvent&) {
    autoDetectFromApi();
}

void DigiBattle99CardEditDialog::onNextSetNo(wxCommandEvent&) {
    if (uniqueSetNos_.size() <= 1) return;
    setNoRingPos_ = (setNoRingPos_ + 1) % uniqueSetNos_.size();
    if (setNoCtrl_) {
        setNoCtrl_->ChangeValue(wxString::FromUTF8(uniqueSetNos_[setNoRingPos_].c_str()));
    }
    refreshVariantNextControls();
}

void DigiBattle99CardEditDialog::autoDetectFromApi() {
    syncCardFromControls();
    const auto& card = constCard();
    if (card.set.name.empty()) {
        showThemedMessageDialog(this, "Select a set first.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return;
    }

    std::string name = card.name;
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) {
        name.erase(name.begin());
    }
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) {
        name.pop_back();
    }

    const std::string setNo =
        setNoCtrl_ ? storedSetNoFromControls(setNoCtrl_) : std::string();
    const bool nameEmpty = name.empty();
    const bool setNoEmpty = setNo.empty();
    const unsigned epoch = variantFetchEpoch_;

    if (nameEmpty && setNoEmpty) {
        showThemedMessageDialog(this, "Enter a card name or set number.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return;
    }

    if (shouldDetectBySetNo(nameEmpty, setNoEmpty)) {
        requestBySetNoAsync(epoch, card.set.name, setNo, true);
        return;
    }

    requestVariantsAsync(epoch, name, card.set.name, true, true);
}

void DigiBattle99CardEditDialog::onSetSelectionChanged(wxCommandEvent& ev) {
    clearCachedPrintVariants();
    scheduleDeferredVariantPrefetch();
    ev.Skip();
}

}  // namespace ccm::ui
