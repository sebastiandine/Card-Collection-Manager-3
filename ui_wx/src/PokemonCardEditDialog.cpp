#include "ccm/ui/PokemonCardEditDialog.hpp"

#include "ccm/domain/Enums.hpp"
#include "ccm/games/pokemonjp/JapanesePokemonCardPreviewSource.hpp"
#include "ccm/ui/Theme.hpp"
#include "ccm/ui/VariantImagePreviewDialog.hpp"

#include <wx/app.h>
#include <wx/panel.h>

#include <thread>
#include <unordered_set>

namespace ccm::ui {
namespace {

constexpr const char* kUnnumberedPromoSetId = "UnnumberedPromo";

constexpr const char* kJapanesePokemonCardBackUrl =
    "https://archives.bulbagarden.net/media/upload/2/2a/TCG_Card_Back_Japanese.jpg";

const std::vector<Set> kEmptySets;

bool languageAllowedForRegion(Language lang, PokemonRegion region) {
    for (const auto l : languagesForPokemonRegion(region)) {
        if (l == lang) return true;
    }
    return false;
}

}  // namespace

PokemonCardEditDialog::PokemonCardEditDialog(wxWindow* parent,
                                             ImageService& imageService,
                                             SetService& setService,
                                             CardPreviewService& cardPreview,
                                             EditMode mode,
                                             PokemonCard initial,
                                             const std::vector<Set>* westSets,
                                             const std::vector<Set>* asiaSets)
    : BaseCardEditDialog<PokemonCard>(
          parent,
          mode == EditMode::Create ? "Add Pokemon Card" : "Edit Pokemon Card",
          imageService, setService, mode, PokemonCard{}, Game::Pokemon, nullptr),
      dialogMode_(mode),
      cardPreview_(cardPreview),
      westSets_(westSets),
      asiaSets_(asiaSets),
      variantFetchState_(std::make_shared<VariantFetchState>()) {
    const PokemonRegion region = initial.region;
    mutableCard() = std::move(initial);
    setPreloadedSetsPointer(region == PokemonRegion::Asia ? asiaSets_ : westSets_);
    buildAndPopulate();
    refreshSetNoRowMode();
    if (dialogMode_ == EditMode::Edit) {
        scheduleDeferredVariantPrefetch();
    }
}

PokemonCardEditDialog::~PokemonCardEditDialog() {
    if (variantFetchState_) {
        variantFetchState_->alive.store(false);
    }
    closeUnnumberedPreview();
}

PokemonRegion PokemonCardEditDialog::currentRegion() const noexcept {
    return constCard().region;
}

Game PokemonCardEditDialog::backendGame() const noexcept {
    return pokemonBackendGame(currentRegion());
}

std::span<const Language> PokemonCardEditDialog::languagesForChoice() const {
    return languagesForPokemonRegion(currentRegion());
}

void PokemonCardEditDialog::onCardLookupContextChanged() {
    clearCachedPrintVariants();
}

void PokemonCardEditDialog::appendPreSetRows(wxFlexGridSizer* grid) {
    auto* regionPanel = new wxPanel(this, wxID_ANY);
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    regionSwitch_ = new SwitchCtrl(regionPanel, wxID_ANY,
                                   constCard().region == PokemonRegion::Asia);
    row->Add(new wxStaticText(regionPanel, wxID_ANY, wxString::FromUTF8("West")),
             0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    row->Add(regionSwitch_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    row->Add(new wxStaticText(regionPanel, wxID_ANY, wxString::FromUTF8("Asia")),
             0, wxALIGN_CENTER_VERTICAL);
    regionPanel->SetSizer(row);
    regionSwitch_->Bind(EVT_CCM_SWITCH, &PokemonCardEditDialog::onRegionSwitch, this);
    appendRow(grid, "Region", regionPanel);
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
    setNoLabel_ = new wxStaticText(this, wxID_ANY, "Set #");
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

    grid->Add(setNoLabel_, 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(setNoPanel, 1, wxEXPAND);

    if (auto* setCombo = setComboControl()) {
        setCombo->Bind(wxEVT_COMBOBOX, &PokemonCardEditDialog::onSetSelectionChanged, this);
    }
}

void PokemonCardEditDialog::onRegionSwitch(wxCommandEvent&) {
    const PokemonRegion next =
        (regionSwitch_ && regionSwitch_->GetValue()) ? PokemonRegion::Asia
                                                     : PokemonRegion::West;
    applyRegion(next, true);
}

void PokemonCardEditDialog::applyRegion(PokemonRegion region, bool clearSetIfMissing) {
    mutableCard().region = region;
    if (regionSwitch_ && regionSwitch_->GetValue() != (region == PokemonRegion::Asia)) {
        regionSwitch_->SetValue(region == PokemonRegion::Asia, false);
    }

    if (!languageAllowedForRegion(mutableCard().language, region)) {
        mutableCard().language = defaultLanguageForPokemonRegion(region);
    }

    const std::vector<Set>* sets =
        region == PokemonRegion::Asia
            ? (asiaSets_ != nullptr ? asiaSets_ : &kEmptySets)
            : (westSets_ != nullptr ? westSets_ : &kEmptySets);
    setPreloadedSetsPointer(sets);

    const std::string prevSetId = mutableCard().set.id;
    bool setStillValid = false;
    for (const auto& s : availableSets()) {
        if (s.id == prevSetId) {
            setStillValid = true;
            mutableCard().set = s;
            break;
        }
    }
    if (clearSetIfMissing && !setStillValid) {
        mutableCard().set = Set{};
        mutableCard().setNo.clear();
        selectedSetNo_.clear();
        if (setNoCtrl_) setNoCtrl_->ChangeValue(wxEmptyString);
    }

    refreshSetAndLanguageChoices();
    clearCachedPrintVariants();
    refreshSetNoRowMode();
    scheduleDeferredVariantPrefetch();
    Layout();
    if (GetSizer()) Fit();
}

std::string PokemonCardEditDialog::normalizedStoredSetNo(std::string_view setNo) {
    return JapanesePokemonCardPreviewSource::normalizeLocalId(setNo);
}

std::string PokemonCardEditDialog::storedSetNoFromControls(const wxTextCtrl* ctrl) {
    if (ctrl == nullptr) return {};
    return normalizedStoredSetNo(ctrl->GetValue().ToStdString(wxConvUTF8));
}

bool PokemonCardEditDialog::isUnnumberedPromoSelected() const {
    if (currentRegion() != PokemonRegion::Asia) return false;
    if (const auto* set = selectedSetFromControls()) {
        return set->id == kUnnumberedPromoSetId;
    }
    return constCard().set.id == kUnnumberedPromoSetId;
}

std::string PokemonCardEditDialog::currentRingSetNo() const {
    if (!uniqueSetNos_.empty() && setNoRingPos_ < uniqueSetNos_.size()) {
        return uniqueSetNos_[setNoRingPos_];
    }
    return selectedSetNo_;
}

void PokemonCardEditDialog::applySelectedSetNo(std::string setNo) {
    selectedSetNo_ = normalizedStoredSetNo(setNo);
    if (setNoCtrl_ && setNoCtrl_->IsShown()) {
        setNoCtrl_->ChangeValue(wxString::FromUTF8(selectedSetNo_.c_str()));
    }
}

void PokemonCardEditDialog::refreshSetNoRowMode() {
    const bool unnumbered = isUnnumberedPromoSelected();
    if (setNoLabel_) {
        setNoLabel_->SetLabelText(unnumbered ? wxString::FromUTF8("Print")
                                            : wxString::FromUTF8("Set #"));
    }
    if (setNoCtrl_) {
        setNoCtrl_->Show(!unnumbered);
        if (!unnumbered && !selectedSetNo_.empty()) {
            setNoCtrl_->ChangeValue(wxString::FromUTF8(selectedSetNo_.c_str()));
        }
        if (auto* parent = setNoCtrl_->GetParent()) {
            parent->Layout();
        }
    }
    if (!unnumbered) {
        closeUnnumberedPreview();
    }
    Layout();
    if (GetSizer()) Fit();
}

void PokemonCardEditDialog::readExtraFromCard() {
    clearCachedPrintVariants();
    selectedSetNo_ = normalizedStoredSetNo(constCard().setNo);
    if (setNoCtrl_) {
        setNoCtrl_->ChangeValue(wxString::FromUTF8(selectedSetNo_.c_str()));
    }
    if (holoCheck_)         holoCheck_->SetValue(constCard().holo);
    if (firstEditionCheck_) firstEditionCheck_->SetValue(constCard().firstEdition);
    if (signedCheck_)       signedCheck_->SetValue(constCard().signed_);
    if (alteredCheck_)      alteredCheck_->SetValue(constCard().altered);
    if (regionSwitch_) {
        regionSwitch_->SetValue(constCard().region == PokemonRegion::Asia, false);
    }
    refreshSetNoRowMode();
}

void PokemonCardEditDialog::writeExtraToCard() {
    mutableCard().region =
        (regionSwitch_ && regionSwitch_->GetValue()) ? PokemonRegion::Asia
                                                     : PokemonRegion::West;
    if (isUnnumberedPromoSelected()) {
        mutableCard().setNo = normalizedStoredSetNo(selectedSetNo_);
    } else if (setNoCtrl_) {
        selectedSetNo_ = storedSetNoFromControls(setNoCtrl_);
        mutableCard().setNo = selectedSetNo_;
    } else {
        mutableCard().setNo = normalizedStoredSetNo(selectedSetNo_);
    }
    if (holoCheck_)         mutableCard().holo         = holoCheck_->IsChecked();
    if (firstEditionCheck_) mutableCard().firstEdition = firstEditionCheck_->IsChecked();
    if (signedCheck_)       mutableCard().signed_      = signedCheck_->IsChecked();
    if (alteredCheck_)      mutableCard().altered      = alteredCheck_->IsChecked();
}

void PokemonCardEditDialog::clearCachedPrintVariants() {
    ++variantFetchEpoch_;
    ++previewFetchEpoch_;
    cachedVariants_.clear();
    uniqueSetNos_.clear();
    setNoRingPos_ = 0;
    closeUnnumberedPreview();
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
    const Game game = backendGame();
    std::thread([state, svc, self, capturedEpoch, name = std::move(name),
                 setId = std::move(setId), fillSetNoOnSuccess, showFailureDialog, game]() {
        auto detected = svc->detectPrintVariants(game, name, setId);
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
    if (fillSetNoOnSuccess && !cachedVariants_.empty()) {
        applySelectedSetNo(cachedVariants_.front().setNo);
    }

    rebuildVariantRingFromCache();
    syncRingPositionToControls();
    refreshVariantNextControls();
    if (isUnnumberedPromoSelected() && !uniqueSetNos_.empty()) {
        ensureUnnumberedPreviewOpen();
        refreshUnnumberedPreview();
        if (unnumberedPreview_ != nullptr) {
            unnumberedPreview_->setNavigationEnabled(uniqueSetNos_.size() > 1);
        }
    }
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
    const std::string current = isUnnumberedPromoSelected()
                                    ? normalizedStoredSetNo(selectedSetNo_)
                                    : storedSetNoFromControls(setNoCtrl_);
    if (!isUnnumberedPromoSelected() && setNoCtrl_) {
        selectedSetNo_ = current;
    }
    setNoRingPos_ = 0;
    for (std::size_t i = 0; i < uniqueSetNos_.size(); ++i) {
        if (uniqueSetNos_[i] == current) {
            setNoRingPos_ = i;
            break;
        }
    }
    if (!uniqueSetNos_.empty() && selectedSetNo_.empty()) {
        applySelectedSetNo(uniqueSetNos_[setNoRingPos_]);
    }
}

void PokemonCardEditDialog::refreshVariantNextControls() {
    if (!nextSetNoBtn_) return;
    const bool showNext = uniqueSetNos_.size() > 1;
    nextSetNoBtn_->Show(showNext);
    if (showNext) {
        if (isUnnumberedPromoSelected()) {
            const std::size_t i = setNoRingPos_ + 1;
            const std::size_t n = uniqueSetNos_.size();
            nextSetNoBtn_->SetLabel(wxString::Format("Next (%zu/%zu)", i, n));
        } else {
            const std::string setNo = currentRingSetNo();
            if (setNo.empty()) {
                nextSetNoBtn_->SetLabel("Next");
            } else {
                nextSetNoBtn_->SetLabel(
                    wxString::Format("Next (%s)", wxString::FromUTF8(setNo.c_str())));
            }
        }
    } else {
        nextSetNoBtn_->SetLabel("Next");
    }
    if (auto* parent = nextSetNoBtn_->GetParent()) {
        parent->Layout();
    }
    Layout();
    if (GetSizer()) Fit();
    if (unnumberedPreview_ != nullptr) {
        unnumberedPreview_->setNavigationEnabled(uniqueSetNos_.size() > 1);
    }
}

void PokemonCardEditDialog::onAutoDetectSetNo(wxCommandEvent&) {
    autoDetectFromApi();
}

void PokemonCardEditDialog::stepVariantRing(int delta) {
    if (uniqueSetNos_.size() <= 1 || delta == 0) return;
    const auto n = static_cast<int>(uniqueSetNos_.size());
    auto pos = static_cast<int>(setNoRingPos_) + delta;
    pos %= n;
    if (pos < 0) pos += n;
    setNoRingPos_ = static_cast<std::size_t>(pos);
    applySelectedSetNo(uniqueSetNos_[setNoRingPos_]);
    refreshVariantNextControls();
    if (isUnnumberedPromoSelected()) {
        ensureUnnumberedPreviewOpen();
        refreshUnnumberedPreview();
        if (unnumberedPreview_ != nullptr) {
            unnumberedPreview_->setNavigationEnabled(true);
        }
    }
}

void PokemonCardEditDialog::onNextSetNo(wxCommandEvent&) {
    stepVariantRing(1);
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
    refreshSetNoRowMode();
    scheduleDeferredVariantPrefetch();
    ev.Skip();
}

void PokemonCardEditDialog::closeUnnumberedPreview() {
    ++previewFetchEpoch_;
    if (unnumberedPreview_ != nullptr) {
        unnumberedPreview_->Destroy();
        unnumberedPreview_ = nullptr;
    }
}

void PokemonCardEditDialog::ensureUnnumberedPreviewOpen() {
    if (!isUnnumberedPromoSelected()) {
        closeUnnumberedPreview();
        return;
    }
    if (unnumberedPreview_ != nullptr) {
        unnumberedPreview_->setNavigationEnabled(uniqueSetNos_.size() > 1);
        unnumberedPreview_->repositionBesideParent();
        return;
    }

    unnumberedPreview_ = new VariantImagePreviewDialog(this);
    const Theme theme = inferThemeFromWindow(this);
    applyThemeToWindowTree(unnumberedPreview_, paletteForTheme(theme), theme);
    unnumberedPreview_->Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& ev) {
        unnumberedPreview_ = nullptr;
        ev.Skip();
    });
    unnumberedPreview_->Bind(EVT_VARIANT_PREVIEW_PREV, [this](wxCommandEvent&) {
        stepVariantRing(-1);
    });
    unnumberedPreview_->Bind(EVT_VARIANT_PREVIEW_NEXT, [this](wxCommandEvent&) {
        stepVariantRing(1);
    });
    unnumberedPreview_->setNavigationEnabled(uniqueSetNos_.size() > 1);
    unnumberedPreview_->Show(true);
    unnumberedPreview_->repositionBesideParent();
}

void PokemonCardEditDialog::refreshUnnumberedPreview() {
    if (!isUnnumberedPromoSelected() || uniqueSetNos_.empty()) {
        closeUnnumberedPreview();
        return;
    }
    ensureUnnumberedPreviewOpen();
    if (unnumberedPreview_ == nullptr) return;

    syncCardFromControls();
    const auto& card = constCard();
    const std::string setNo = currentRingSetNo();
    if (card.name.empty() || setNo.empty()) {
        unnumberedPreview_->clearImage();
        unnumberedPreview_->setCaption(wxString::FromUTF8("Enter a card name"));
        return;
    }

    const std::size_t i = setNoRingPos_ + 1;
    const std::size_t n = uniqueSetNos_.size();
    unnumberedPreview_->setCaption(
        wxString::Format("%s  (%zu/%zu)",
                         wxString::FromUTF8(card.name.c_str()), i, n));

    const unsigned epoch = ++previewFetchEpoch_;
    requestUnnumberedPreviewAsync(epoch, card.name, kUnnumberedPromoSetId, setNo, setNoRingPos_,
                                  uniqueSetNos_.size());
}

void PokemonCardEditDialog::requestUnnumberedPreviewAsync(unsigned capturedEpoch,
                                                          std::string name,
                                                          std::string setId,
                                                          std::string setNo,
                                                          std::size_t ringIndex,
                                                          std::size_t ringCount) {
    auto state = variantFetchState_;
    CardPreviewService* svc = &cardPreview_;
    PokemonCardEditDialog* self = this;
    std::thread([state, svc, self, capturedEpoch, name = std::move(name),
                 setId = std::move(setId), setNo = std::move(setNo), ringIndex,
                 ringCount]() {
        auto bytes = svc->fetchPreviewBytes(Game::JapanesePokemon, name, setId, setNo);
        std::string payload;
        bool usedFallback = false;
        if (bytes) {
            payload = std::move(bytes).value();
        } else {
            auto fallback = svc->fetchImageBytesByUrl(kJapanesePokemonCardBackUrl);
            if (fallback) {
                payload = std::move(fallback).value();
                usedFallback = true;
            }
        }
        wxTheApp->CallAfter([state, self, capturedEpoch, payload = std::move(payload),
                             name, ringIndex, ringCount, usedFallback]() mutable {
            if (!state->alive.load()) return;
            if (capturedEpoch != self->previewFetchEpoch_) return;
            if (self->unnumberedPreview_ == nullptr) return;

            self->unnumberedPreview_->setImageBytes(payload);
            if (usedFallback) {
                self->unnumberedPreview_->setCaption(
                    wxString::Format("%s  (%zu/%zu) — preview unavailable",
                                     wxString::FromUTF8(name.c_str()),
                                     ringIndex + 1, ringCount));
            }
        });
    }).detach();
}

}  // namespace ccm::ui
