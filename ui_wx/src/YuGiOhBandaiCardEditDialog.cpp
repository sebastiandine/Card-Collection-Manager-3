#include "ccm/ui/YuGiOhBandaiCardEditDialog.hpp"

#include "ccm/domain/Enums.hpp"
#include "ccm/games/yugiohbandai/YuGiOhBandaiSetSource.hpp"
#include "ccm/ui/Theme.hpp"
#include <wx/app.h>
#include <wx/panel.h>

#include <algorithm>
#include <cctype>
#include <thread>
#include <utility>

namespace ccm::ui {

namespace {
const char* const kRarityOptions[] = {
    "Common",
    "Rare",
    "Super Rare",
    "Ultra Rare",
    "Holo Seal",
};
}  // namespace

YuGiOhBandaiCardEditDialog::YuGiOhBandaiCardEditDialog(wxWindow* parent,
                                                       ImageService& imageService,
                                                       SetService& setService,
                                                       CardPreviewService& cardPreview,
                                                       EditMode mode,
                                                       YuGiOhBandaiCard initial,
                                                       const std::vector<Set>* preloadedSets)
    : BaseCardEditDialog<YuGiOhBandaiCard>(
          parent,
          mode == EditMode::Create ? "Add Yu-Gi-Oh! (Bandai) Card"
                                   : "Edit Yu-Gi-Oh! (Bandai) Card",
          imageService, setService, mode, std::move(initial), Game::YuGiOhBandai,
          preloadedSets),
      dialogMode_(mode),
      cardPreview_(cardPreview),
      variantFetchState_(std::make_shared<VariantFetchState>()) {
    buildAndPopulate();
    if (dialogMode_ == EditMode::Edit) {
        scheduleDeferredVariantPrefetch();
    }
}

YuGiOhBandaiCardEditDialog::~YuGiOhBandaiCardEditDialog() {
    if (variantFetchState_) {
        variantFetchState_->alive.store(false);
    }
}

std::span<const Language> YuGiOhBandaiCardEditDialog::languagesForChoice() const {
    static constexpr Language kLangs[] = {Language::Japanese, Language::English};
    return kLangs;
}

void YuGiOhBandaiCardEditDialog::onCardLookupContextChanged() {
    clearCachedPrintVariants();
}

bool YuGiOhBandaiCardEditDialog::validateExtraFields() {
    const std::string setNo =
        YuGiOhBandaiSetSource::normalizeCardNumber(constCard().setNo);
    if (setNo.empty()) {
        showThemedMessageDialog(
            this,
            "Set number (No.) is required for set completion tracking.\n"
            "Enter a Bandai number or use Auto detect.",
            "Add card", wxOK | wxICON_INFORMATION);
        return false;
    }
    // Persist the normalized form so ownership keys stay stable.
    mutableCard().setNo = setNo;
    if (setNoCtrl_) setNoCtrl_->ChangeValue(wxString::FromUTF8(setNo.c_str()));
    return true;
}

void YuGiOhBandaiCardEditDialog::buildFlagsRow(wxBoxSizer* flagsBox) {
    holoCheck_    = new wxCheckBox(this, wxID_ANY, "Holo");
    signedCheck_  = new wxCheckBox(this, wxID_ANY, "Signed");
    alteredCheck_ = new wxCheckBox(this, wxID_ANY, "Altered");
    flagsBox->Add(holoCheck_,    0, wxRIGHT, 12);
    flagsBox->Add(signedCheck_,  0, wxRIGHT, 12);
    flagsBox->Add(alteredCheck_, 0, wxRIGHT, 12);
}

void YuGiOhBandaiCardEditDialog::appendExtraRows(wxFlexGridSizer* grid) {
    auto* setNoPanel = new wxPanel(this, wxID_ANY);
    setNoCtrl_ = new wxTextCtrl(setNoPanel, wxID_ANY);
    autoSetNoBtn_ = new wxButton(setNoPanel, wxID_ANY, "Auto detect");
    autoSetNoBtn_->Bind(wxEVT_BUTTON, &YuGiOhBandaiCardEditDialog::onAutoDetectBySetNo, this);
    nextSetNoBtn_ = new wxButton(setNoPanel, wxID_ANY, "Next");
    nextSetNoBtn_->Bind(wxEVT_BUTTON, &YuGiOhBandaiCardEditDialog::onNextSetNo, this);
    nextSetNoBtn_->Show(false);
    setNoCtrl_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { markSetNoLookupEdited(); });
    auto* setNoRow = new wxBoxSizer(wxHORIZONTAL);
    setNoRow->Add(setNoCtrl_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    setNoRow->Add(autoSetNoBtn_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    setNoRow->Add(nextSetNoBtn_, 0, wxALIGN_CENTER_VERTICAL);
    setNoPanel->SetSizer(setNoRow);

    auto* rarityPanel = new wxPanel(this, wxID_ANY);
    rarityChoice_ = new wxChoice(rarityPanel, wxID_ANY);
    wxArrayString rarityItems;
    rarityItems.Alloc(static_cast<int>(sizeof(kRarityOptions) / sizeof(kRarityOptions[0])));
    for (const char* rarity : kRarityOptions) {
        rarityItems.Add(wxString::FromUTF8(rarity));
    }
    rarityChoice_->Append(rarityItems);
    rarityChoice_->Bind(wxEVT_CHOICE, &YuGiOhBandaiCardEditDialog::onRarityChoiceChanged, this);
    autoRarityBtn_ = new wxButton(rarityPanel, wxID_ANY, "Auto detect");
    autoRarityBtn_->Bind(wxEVT_BUTTON, &YuGiOhBandaiCardEditDialog::onAutoDetectByName, this);
    auto* rarityRow = new wxBoxSizer(wxHORIZONTAL);
    rarityRow->Add(rarityChoice_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    rarityRow->Add(autoRarityBtn_, 0, wxALIGN_CENTER_VERTICAL);
    rarityPanel->SetSizer(rarityRow);

    appendRow(grid, "No.", setNoPanel);
    appendRow(grid, "Rarity", rarityPanel);

    if (auto* setCombo = setComboControl()) {
        setCombo->Bind(wxEVT_COMBOBOX, &YuGiOhBandaiCardEditDialog::onSetSelectionChanged, this);
    }
}

void YuGiOhBandaiCardEditDialog::readExtraFromCard() {
    clearCachedPrintVariants();
    if (setNoCtrl_) setNoCtrl_->ChangeValue(wxString::FromUTF8(constCard().setNo.c_str()));
    applyRarityStringToChoice(constCard().rarity);
    if (holoCheck_)    holoCheck_->SetValue(constCard().holo);
    if (signedCheck_)  signedCheck_->SetValue(constCard().signed_);
    if (alteredCheck_) alteredCheck_->SetValue(constCard().altered);
}

void YuGiOhBandaiCardEditDialog::writeExtraToCard() {
    if (setNoCtrl_)    mutableCard().setNo   = setNoCtrl_->GetValue().ToStdString(wxConvUTF8);
    if (rarityChoice_) mutableCard().rarity  = rarityChoice_->GetStringSelection().ToStdString(wxConvUTF8);
    if (holoCheck_)    mutableCard().holo    = holoCheck_->IsChecked();
    if (signedCheck_)  mutableCard().signed_ = signedCheck_->IsChecked();
    if (alteredCheck_) mutableCard().altered = alteredCheck_->IsChecked();
}

void YuGiOhBandaiCardEditDialog::applyRarityStringToChoice(const std::string& rarity) {
    if (!rarityChoice_) return;
    if (rarity.empty()) {
        rarityChoice_->SetSelection(0);
        return;
    }
    const wxString wxRare = wxString::FromUTF8(rarity.c_str());
    int idx = rarityChoice_->FindString(wxRare);
    if (idx == wxNOT_FOUND) {
        rarityChoice_->Append(wxRare);
        idx = rarityChoice_->GetCount() - 1;
    }
    if (idx != wxNOT_FOUND) rarityChoice_->SetSelection(idx);
}

void YuGiOhBandaiCardEditDialog::maybeAutoCheckHoloForRarity(const std::string& rarity) {
    if (rarity == "Holo Seal" && holoCheck_ != nullptr) {
        holoCheck_->SetValue(true);
    }
}

void YuGiOhBandaiCardEditDialog::onRarityChoiceChanged(wxCommandEvent&) {
    if (!rarityChoice_) return;
    maybeAutoCheckHoloForRarity(rarityChoice_->GetStringSelection().ToStdString(wxConvUTF8));
}

std::size_t YuGiOhBandaiCardEditDialog::findAvailableSetIndex(const std::string& setId) const {
    const auto& available = availableSets();
    auto lowerAscii = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return s;
    };
    const std::string target = lowerAscii(setId);
    for (std::size_t i = 0; i < available.size(); ++i) {
        if (lowerAscii(available[i].id) == target) return i;
    }
    return available.size();
}

void YuGiOhBandaiCardEditDialog::applyDetectedPrint(const AutoDetectedPrint& print) {
    if (!print.name.empty()) {
        if (auto* name = nameControl()) {
            name->ChangeValue(wxString::FromUTF8(print.name.c_str()));
        }
    }
    if (!print.setNo.empty() && setNoCtrl_) {
        setNoCtrl_->ChangeValue(wxString::FromUTF8(print.setNo.c_str()));
    }
    if (!print.rarity.empty()) {
        applyRarityStringToChoice(print.rarity);
        maybeAutoCheckHoloForRarity(print.rarity);
    }
    if (!print.language.empty()) {
        if (auto lang = languageFromString(print.language)) {
            for (const Language l : languagesForChoice()) {
                if (l != *lang) continue;
                if (auto* choice = languageChoiceControl()) {
                    const wxString wanted =
                        wxString::FromUTF8(std::string(to_string(*lang)).c_str());
                    const int idx = choice->FindString(wanted);
                    if (idx != wxNOT_FOUND) choice->SetSelection(idx);
                }
                break;
            }
        }
    }
    if (!print.setId.empty()) {
        const std::size_t idx = findAvailableSetIndex(print.setId);
        if (idx < availableSets().size()) {
            applySetSelectionByIndex(idx);
        }
    }
}

void YuGiOhBandaiCardEditDialog::clearCachedPrintVariants() {
    ++variantFetchEpoch_;
    cachedVariants_.clear();
    variantRingPos_ = 0;
    refreshVariantNextControls();
}

void YuGiOhBandaiCardEditDialog::refreshVariantNextControls() {
    if (!nextSetNoBtn_) return;
    nextSetNoBtn_->Show(cachedVariants_.size() > 1);
    Layout();
    if (GetSizer()) Fit();
}

void YuGiOhBandaiCardEditDialog::scheduleDeferredVariantPrefetch() {
    const unsigned epoch = variantFetchEpoch_;
    wxTheApp->CallAfter([this, epoch]() {
        prefetchVariantsForCurrentCardSilent(epoch);
    });
}

void YuGiOhBandaiCardEditDialog::prefetchVariantsForCurrentCardSilent(unsigned capturedEpoch) {
    if (capturedEpoch != variantFetchEpoch_) return;
    if (!cachedVariants_.empty()) return;
    const auto& card = constCard();
    if (card.name.empty()) return;
    requestByNameAsync(capturedEpoch, card.name, card.set.id, false);
}

void YuGiOhBandaiCardEditDialog::requestByNameAsync(unsigned capturedEpoch, std::string name,
                                                    std::string setId, bool showFailureDialog) {
    if (capturedEpoch != variantFetchEpoch_) return;
    if (showFailureDialog && autoRarityBtn_) autoRarityBtn_->Disable();

    auto state = variantFetchState_;
    CardPreviewService* svc = &cardPreview_;
    YuGiOhBandaiCardEditDialog* self = this;
    std::thread([state, svc, self, capturedEpoch, name = std::move(name),
                 setId = std::move(setId), showFailureDialog]() {
        auto detected = svc->detectPrintVariants(Game::YuGiOhBandai, name, setId);
        wxTheApp->CallAfter([state, self, capturedEpoch, detected = std::move(detected),
                             showFailureDialog]() mutable {
            if (!state->alive.load()) return;
            self->applyDetectedList(capturedEpoch, std::move(detected), showFailureDialog,
                                    /*applyFirst=*/true);
        });
    }).detach();
}

void YuGiOhBandaiCardEditDialog::requestByNoAsync(unsigned capturedEpoch, std::string setId,
                                                  std::string setNo, bool showFailureDialog) {
    if (capturedEpoch != variantFetchEpoch_) return;
    if (showFailureDialog && autoSetNoBtn_) autoSetNoBtn_->Disable();

    auto state = variantFetchState_;
    CardPreviewService* svc = &cardPreview_;
    YuGiOhBandaiCardEditDialog* self = this;
    std::thread([state, svc, self, capturedEpoch, setId = std::move(setId),
                 setNo = std::move(setNo), showFailureDialog]() {
        auto detected = svc->detectVariantsBySetNo(Game::YuGiOhBandai, setId, setNo);
        wxTheApp->CallAfter([state, self, capturedEpoch, detected = std::move(detected),
                             showFailureDialog]() mutable {
            if (!state->alive.load()) return;
            self->applyDetectedList(capturedEpoch, std::move(detected), showFailureDialog,
                                    /*applyFirst=*/true);
        });
    }).detach();
}

void YuGiOhBandaiCardEditDialog::applyDetectedList(
    unsigned capturedEpoch,
    Result<std::vector<AutoDetectedPrint>> detected,
    bool showFailureDialog,
    bool applyFirst) {
    if (capturedEpoch != variantFetchEpoch_) return;

    if (autoSetNoBtn_)  autoSetNoBtn_->Enable();
    if (autoRarityBtn_) autoRarityBtn_->Enable();

    if (!detected) {
        if (showFailureDialog) {
            showThemedMessageDialog(this, "Auto detect failed: " + detected.error(),
                                    "Auto detect", wxOK | wxICON_WARNING);
        }
        return;
    }
    if (detected.value().empty()) {
        if (showFailureDialog) {
            showThemedMessageDialog(this, "No matching Yu-Gi-Oh! (Bandai) card found.",
                                    "Auto detect", wxOK | wxICON_INFORMATION);
        }
        return;
    }

    cachedVariants_ = std::move(detected).value();
    variantRingPos_ = 0;
    if (applyFirst) applyDetectedPrint(cachedVariants_.front());
    refreshVariantNextControls();
}

void YuGiOhBandaiCardEditDialog::onAutoDetectBySetNo(wxCommandEvent&) {
    syncCardFromControls();
    const auto& card = constCard();
    std::string setId;
    if (const Set* set = selectedSetFromControls()) setId = set->id;
    if (setId.empty()) {
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
        setNoCtrl_ ? setNoCtrl_->GetValue().ToStdString(wxConvUTF8) : std::string();
    const bool nameEmpty = name.empty();
    const bool setNoEmpty =
        YuGiOhBandaiSetSource::normalizeCardNumber(setNo).empty();

    if (nameEmpty && setNoEmpty) {
        showThemedMessageDialog(this, "Enter a card name or Bandai number.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return;
    }

    const unsigned epoch = variantFetchEpoch_;
    if (shouldDetectBySetNo(nameEmpty, setNoEmpty)) {
        requestByNoAsync(epoch, setId, setNo, true);
        return;
    }
    requestByNameAsync(epoch, name, setId, true);
}

void YuGiOhBandaiCardEditDialog::onNextSetNo(wxCommandEvent&) {
    if (cachedVariants_.size() <= 1) return;
    variantRingPos_ = (variantRingPos_ + 1) % cachedVariants_.size();
    applyDetectedPrint(cachedVariants_[variantRingPos_]);
}

void YuGiOhBandaiCardEditDialog::onAutoDetectByName(wxCommandEvent&) {
    syncCardFromControls();
    const auto& card = constCard();
    if (card.name.empty()) {
        showThemedMessageDialog(this, "Enter a card name first.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return;
    }
    std::string setId;
    if (const Set* set = selectedSetFromControls()) setId = set->id;
    if (setId.empty()) {
        showThemedMessageDialog(this, "Select a set first.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return;
    }

    const unsigned epoch = variantFetchEpoch_;
    requestByNameAsync(epoch, card.name, setId, true);
}

void YuGiOhBandaiCardEditDialog::onSetSelectionChanged(wxCommandEvent& ev) {
    clearCachedPrintVariants();
    scheduleDeferredVariantPrefetch();
    ev.Skip();
}

}  // namespace ccm::ui
