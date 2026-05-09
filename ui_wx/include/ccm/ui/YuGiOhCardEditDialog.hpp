#pragma once

#include "ccm/domain/YuGiOhCard.hpp"
#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/ui/BaseCardEditDialog.hpp"
#include <wx/button.h>
#include <wx/stattext.h>

namespace ccm::ui {

class YuGiOhCardEditDialog final : public BaseCardEditDialog<YuGiOhCard> {
public:
    YuGiOhCardEditDialog(wxWindow* parent,
                         ImageService& imageService,
                         SetService& setService,
                         CardPreviewService& cardPreview,
                         EditMode mode,
                         YuGiOhCard initial,
                         const std::vector<Set>* preloadedSets = nullptr);

protected:
    void buildFlagsRow(wxBoxSizer* flagsBox) override;
    void appendExtraRows(wxFlexGridSizer* grid) override;
    void readExtraFromCard() override;
    void writeExtraToCard() override;
    [[nodiscard]] std::string updateMenuName() const override { return "Update Yu-Gi-Oh!"; }
    void onCardLookupContextChanged() override;

private:
    void onAutoDetectSetNo(wxCommandEvent&);
    void onAutoDetectRarity(wxCommandEvent&);
    void onNextSetNo(wxCommandEvent&);
    void onNextRarity(wxCommandEvent&);
    void onSetNoTextChanged(wxCommandEvent&);
    void onSetSelectionChanged(wxCommandEvent&);
    void autoDetectFromApi(bool fillSetNo, bool fillRarity);
    void refreshSetNoFullPreview();
    void clearCachedPrintVariants();
    bool fetchAndCachePrintVariants();
    void rebuildVariantRingsFromCache();
    void filterCachedVariantsForCardLanguage();
    void syncRingPositionsToControls();
    void applyRarityStringToChoice(const std::string& rarity);
    [[nodiscard]] std::string currentFullSetNoFromControls() const;
    void refreshVariantNextControls();
    [[nodiscard]] std::string extractSetNoNumeric(std::string_view fullSetNo) const;
    [[nodiscard]] std::string composeFullSetNo(std::string_view numeric) const;
    void scheduleDeferredVariantPrefetch();
    void prefetchVariantsForCurrentCardSilent(unsigned capturedEpoch);

    EditMode                      dialogMode_;
    unsigned                      variantFetchEpoch_{0};
    CardPreviewService& cardPreview_;
    wxTextCtrl* setNoCtrl_{nullptr};
    wxStaticText* setNoFullPreview_{nullptr};
    wxChoice*   rarityChoice_{nullptr};
    wxButton*   autoSetNoBtn_{nullptr};
    wxButton*   nextSetNoBtn_{nullptr};
    wxButton*   autoRarityBtn_{nullptr};
    wxButton*   nextRarityBtn_{nullptr};
    wxCheckBox* firstEditionCheck_{nullptr};
    wxCheckBox* signedCheck_{nullptr};
    wxCheckBox* alteredCheck_{nullptr};

    std::vector<AutoDetectedPrint> cachedVariants_;
    std::vector<std::string>       uniqueSetCodes_;
    std::vector<std::string>       raritiesForCurrentSetCode_;
    std::size_t setCodeRingPos_{0};
    std::size_t rarityRingPos_{0};
};

}  // namespace ccm::ui
