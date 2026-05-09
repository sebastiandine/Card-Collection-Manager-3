#include "ccm/ui/YuGiOhCardEditDialog.hpp"
#include "ccm/domain/Enums.hpp"
#include "ccm/util/YuGiOhPrintingSlot.hpp"
#include <wx/app.h>
#include <wx/panel.h>
#include <algorithm>
#include <cctype>
#include <unordered_set>

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
      dialogMode_(mode),
      cardPreview_(cardPreview) {
    buildAndPopulate();
    if (dialogMode_ == EditMode::Edit) {
        scheduleDeferredVariantPrefetch();
    }
}

void YuGiOhCardEditDialog::onCardLookupContextChanged() {
    clearCachedPrintVariants();
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
    nextSetNoBtn_ = new wxButton(setNoPanel, wxID_ANY, "Next");
    nextSetNoBtn_->Bind(wxEVT_BUTTON, &YuGiOhCardEditDialog::onNextSetNo, this);
    nextSetNoBtn_->Show(false);
    setNoFullPreview_ = new wxStaticText(setNoPanel, wxID_ANY, "(n/a)");
    auto* setNoRow = new wxBoxSizer(wxHORIZONTAL);
    setNoRow->Add(setNoCtrl_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    setNoRow->Add(autoSetNoBtn_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    setNoRow->Add(nextSetNoBtn_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    setNoRow->Add(setNoFullPreview_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
    setNoPanel->SetSizer(setNoRow);

    auto* rarityPanel = new wxPanel(this, wxID_ANY);
    rarityChoice_ = new wxChoice(rarityPanel, wxID_ANY);
    autoRarityBtn_ = new wxButton(rarityPanel, wxID_ANY, "Auto detect");
    autoRarityBtn_->Bind(wxEVT_BUTTON, &YuGiOhCardEditDialog::onAutoDetectRarity, this);
    nextRarityBtn_ = new wxButton(rarityPanel, wxID_ANY, "Next");
    nextRarityBtn_->Bind(wxEVT_BUTTON, &YuGiOhCardEditDialog::onNextRarity, this);
    nextRarityBtn_->Show(false);
    wxArrayString rarityItems;
    rarityItems.Alloc(static_cast<int>(sizeof(kRarityOptions) / sizeof(kRarityOptions[0])));
    for (const char* rarity : kRarityOptions) {
        rarityItems.Add(wxString::FromUTF8(rarity));
    }
    rarityChoice_->Append(rarityItems);
    auto* rarityRow = new wxBoxSizer(wxHORIZONTAL);
    rarityRow->Add(rarityChoice_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    rarityRow->Add(autoRarityBtn_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    rarityRow->Add(nextRarityBtn_, 0, wxALIGN_CENTER_VERTICAL);
    rarityPanel->SetSizer(rarityRow);

    appendRow(grid, "Set #", setNoPanel);
    appendRow(grid, "Rarity", rarityPanel);

    if (auto* setCombo = setComboControl()) {
        setCombo->Bind(wxEVT_COMBOBOX, &YuGiOhCardEditDialog::onSetSelectionChanged, this);
    }
}

void YuGiOhCardEditDialog::readExtraFromCard() {
    clearCachedPrintVariants();
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
    if (setNoCtrl_)         mutableCard().setNo        = composeFullSetNo(setNoCtrl_->GetValue().ToStdString(wxConvUTF8));
    if (rarityChoice_)      mutableCard().rarity       = rarityChoice_->GetStringSelection().ToStdString(wxConvUTF8);
    if (firstEditionCheck_) mutableCard().firstEdition = firstEditionCheck_->IsChecked();
    if (signedCheck_)       mutableCard().signed_      = signedCheck_->IsChecked();
    if (alteredCheck_)      mutableCard().altered      = alteredCheck_->IsChecked();
}

void YuGiOhCardEditDialog::clearCachedPrintVariants() {
    ++variantFetchEpoch_;
    cachedVariants_.clear();
    uniqueSetCodes_.clear();
    raritiesForCurrentSetCode_.clear();
    setCodeRingPos_ = 0;
    rarityRingPos_ = 0;
    refreshVariantNextControls();
}

void YuGiOhCardEditDialog::scheduleDeferredVariantPrefetch() {
    const unsigned epoch = variantFetchEpoch_;
    wxTheApp->CallAfter([this, epoch]() {
        prefetchVariantsForCurrentCardSilent(epoch);
    });
}

void YuGiOhCardEditDialog::prefetchVariantsForCurrentCardSilent(unsigned capturedEpoch) {
    if (capturedEpoch != variantFetchEpoch_) return;
    if (!cachedVariants_.empty()) return;
    const auto& card = constCard();
    if (card.name.empty() || card.set.name.empty()) return;

    auto detected = cardPreview_.detectPrintVariants(Game::YuGiOh, card.name, card.set.name);
    if (!detected) return;
    if (capturedEpoch != variantFetchEpoch_) return;

    cachedVariants_ = std::move(detected).value();
    rebuildVariantRingsFromCache();
    syncRingPositionsToControls();
    refreshVariantNextControls();
}

bool YuGiOhCardEditDialog::fetchAndCachePrintVariants() {
    syncCardFromControls();
    const auto& card = constCard();
    if (card.name.empty()) {
        showThemedMessageDialog(this, "Enter a card name first.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return false;
    }
    if (card.set.name.empty()) {
        showThemedMessageDialog(this, "Select a set first.", "Auto detect",
                                wxOK | wxICON_INFORMATION);
        return false;
    }

    auto detected = cardPreview_.detectPrintVariants(Game::YuGiOh, card.name, card.set.name);
    if (!detected) {
        showThemedMessageDialog(this, "Auto detect failed: " + detected.error(), "Auto detect",
                                wxOK | wxICON_WARNING);
        return false;
    }
    cachedVariants_ = std::move(detected).value();
    return true;
}

void YuGiOhCardEditDialog::filterCachedVariantsForCardLanguage() {
    if (constCard().language != Language::English) return;
    const auto removed = std::remove_if(
        cachedVariants_.begin(), cachedVariants_.end(), [](const AutoDetectedPrint& p) {
            return ygoLikelyEuropeanRegionalSetCode(p.setNo);
        });
    cachedVariants_.erase(removed, cachedVariants_.end());
}

void YuGiOhCardEditDialog::rebuildVariantRingsFromCache() {
    syncCardFromControls();
    uniqueSetCodes_.clear();
    raritiesForCurrentSetCode_.clear();
    if (cachedVariants_.empty()) return;

    filterCachedVariantsForCardLanguage();

    std::unordered_set<std::string> seenSlots;
    seenSlots.reserve(cachedVariants_.size());
    for (const auto& p : cachedVariants_) {
        if (p.setNo.empty()) continue;
        const std::string digits = ygoCollectorDigitsOnly(p.setNo);
        if (digits.empty()) continue;
        const std::string slotKey = ygoAbbrevBeforeDash(p.setNo) + "|" + digits;
        if (!seenSlots.insert(slotKey).second) continue;
        uniqueSetCodes_.push_back(p.setNo);
    }

    const std::string full = currentFullSetNoFromControls();
    std::unordered_set<std::string> seenRarity;
    for (const auto& p : cachedVariants_) {
        if (!ygoPrintingSlotsMatch(p.setNo, full)) continue;
        if (p.rarity.empty()) continue;
        if (seenRarity.insert(p.rarity).second) raritiesForCurrentSetCode_.push_back(p.rarity);
    }
}

void YuGiOhCardEditDialog::syncRingPositionsToControls() {
    const std::string full = currentFullSetNoFromControls();
    setCodeRingPos_ = 0;
    for (std::size_t i = 0; i < uniqueSetCodes_.size(); ++i) {
        if (ygoPrintingSlotsMatch(uniqueSetCodes_[i], full)) {
            setCodeRingPos_ = i;
            break;
        }
    }

    rarityRingPos_ = 0;
    if (rarityChoice_) {
        const std::string r = rarityChoice_->GetStringSelection().ToStdString(wxConvUTF8);
        for (std::size_t i = 0; i < raritiesForCurrentSetCode_.size(); ++i) {
            if (raritiesForCurrentSetCode_[i] == r) {
                rarityRingPos_ = i;
                break;
            }
        }
    }
}

void YuGiOhCardEditDialog::applyRarityStringToChoice(const std::string& rarity) {
    if (!rarityChoice_) return;
    const wxString wxRare = wxString::FromUTF8(rarity.c_str());
    int idx = rarityChoice_->FindString(wxRare);
    if (idx == wxNOT_FOUND && !rarity.empty()) {
        rarityChoice_->Append(wxRare);
        idx = rarityChoice_->GetCount() - 1;
    }
    if (idx != wxNOT_FOUND) rarityChoice_->SetSelection(idx);
}

std::string YuGiOhCardEditDialog::currentFullSetNoFromControls() const {
    if (!setNoCtrl_) return {};
    return composeFullSetNo(setNoCtrl_->GetValue().ToStdString(wxConvUTF8));
}

void YuGiOhCardEditDialog::refreshVariantNextControls() {
    if (!nextSetNoBtn_ || !nextRarityBtn_) return;
    nextSetNoBtn_->Show(uniqueSetCodes_.size() > 1);
    nextRarityBtn_->Show(raritiesForCurrentSetCode_.size() > 1);
    Layout();
    if (GetSizer()) Fit();
}

void YuGiOhCardEditDialog::onAutoDetectSetNo(wxCommandEvent&) {
    autoDetectFromApi(true, false);
}

void YuGiOhCardEditDialog::onAutoDetectRarity(wxCommandEvent&) {
    autoDetectFromApi(false, true);
}

void YuGiOhCardEditDialog::onNextSetNo(wxCommandEvent&) {
    if (uniqueSetCodes_.size() <= 1) return;
    setCodeRingPos_ = (setCodeRingPos_ + 1) % uniqueSetCodes_.size();
    const std::string& code = uniqueSetCodes_[setCodeRingPos_];
    if (setNoCtrl_) {
        setNoCtrl_->ChangeValue(wxString::FromUTF8(extractSetNoNumeric(code).c_str()));
    }
    for (const auto& p : cachedVariants_) {
        if (p.setNo == code) {
            applyRarityStringToChoice(p.rarity);
            break;
        }
    }
    rebuildVariantRingsFromCache();
    syncRingPositionsToControls();
    refreshSetNoFullPreview();
    refreshVariantNextControls();
}

void YuGiOhCardEditDialog::onNextRarity(wxCommandEvent&) {
    if (raritiesForCurrentSetCode_.size() <= 1) return;
    rarityRingPos_ = (rarityRingPos_ + 1) % raritiesForCurrentSetCode_.size();
    applyRarityStringToChoice(raritiesForCurrentSetCode_[rarityRingPos_]);
    refreshVariantNextControls();
}

void YuGiOhCardEditDialog::autoDetectFromApi(bool fillSetNo, bool fillRarity) {
    if (!fetchAndCachePrintVariants()) return;

    if (fillSetNo && setNoCtrl_ && !cachedVariants_.empty()) {
        const auto& p = cachedVariants_.front();
        setNoCtrl_->ChangeValue(wxString::FromUTF8(extractSetNoNumeric(p.setNo).c_str()));
        if (fillRarity) applyRarityStringToChoice(p.rarity);
    } else if (fillRarity && rarityChoice_) {
        const std::string full = currentFullSetNoFromControls();
        bool applied = false;
        for (const auto& p : cachedVariants_) {
            if (ygoPrintingSlotsMatch(p.setNo, full)) {
                applyRarityStringToChoice(p.rarity);
                applied = true;
                break;
            }
        }
        if (!applied && !cachedVariants_.empty()) {
            applyRarityStringToChoice(cachedVariants_.front().rarity);
        }
    }

    rebuildVariantRingsFromCache();
    syncRingPositionsToControls();
    refreshSetNoFullPreview();
    refreshVariantNextControls();
}

void YuGiOhCardEditDialog::onSetNoTextChanged(wxCommandEvent&) {
    refreshSetNoFullPreview();
    if (!cachedVariants_.empty()) {
        rebuildVariantRingsFromCache();
        syncRingPositionsToControls();
        refreshVariantNextControls();
    }
}

void YuGiOhCardEditDialog::onSetSelectionChanged(wxCommandEvent& ev) {
    clearCachedPrintVariants();
    refreshSetNoFullPreview();
    scheduleDeferredVariantPrefetch();
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
    const std::string full = composeFullSetNo(setNoCtrl_->GetValue().ToStdString(wxConvUTF8));
    if (full.empty()) {
        setNoFullPreview_->SetLabel("(n/a)");
    } else {
        setNoFullPreview_->SetLabel(wxString::Format("(%s)", wxString::FromUTF8(full.c_str())));
    }
    setNoFullPreview_->GetParent()->Layout();
}

}  // namespace ccm::ui
