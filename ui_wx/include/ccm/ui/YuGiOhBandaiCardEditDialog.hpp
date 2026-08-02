#pragma once

#include "ccm/domain/YuGiOhBandaiCard.hpp"
#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/ui/BaseCardEditDialog.hpp"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>

#include <atomic>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ccm::ui {

class YuGiOhBandaiCardEditDialog final : public BaseCardEditDialog<YuGiOhBandaiCard> {
public:
    YuGiOhBandaiCardEditDialog(wxWindow* parent,
                               ImageService& imageService,
                               SetService& setService,
                               CardPreviewService& cardPreview,
                               EditMode mode,
                               YuGiOhBandaiCard initial,
                               const std::vector<Set>* preloadedSets = nullptr);
    ~YuGiOhBandaiCardEditDialog() override;

protected:
    void buildFlagsRow(wxBoxSizer* flagsBox) override;
    void appendExtraRows(wxFlexGridSizer* grid) override;
    void readExtraFromCard() override;
    void writeExtraToCard() override;
    [[nodiscard]] std::span<const Language> languagesForChoice() const override;
    [[nodiscard]] std::string updateMenuName() const override {
        return "Update Yu-Gi-Oh! (Bandai)";
    }
    void onCardLookupContextChanged() override;
    [[nodiscard]] bool validateExtraFields() override;

private:
    struct VariantFetchState {
        std::atomic<bool> alive{true};
    };

    void onAutoDetectBySetNo(wxCommandEvent&);
    void onNextSetNo(wxCommandEvent&);
    void onAutoDetectByName(wxCommandEvent&);
    void onRarityChoiceChanged(wxCommandEvent&);
    void onSetSelectionChanged(wxCommandEvent&);

    void scheduleDeferredVariantPrefetch();
    void prefetchVariantsForCurrentCardSilent(unsigned capturedEpoch);
    void requestByNameAsync(unsigned capturedEpoch, std::string name, std::string setId,
                            bool showFailureDialog);
    void requestByNoAsync(unsigned capturedEpoch, std::string setId, std::string setNo,
                          bool showFailureDialog);
    void applyDetectedList(unsigned capturedEpoch,
                           Result<std::vector<AutoDetectedPrint>> detected,
                           bool showFailureDialog, bool applyFirst);
    void clearCachedPrintVariants();
    void applyDetectedPrint(const AutoDetectedPrint& print);
    void applyRarityStringToChoice(const std::string& rarity);
    void maybeAutoCheckHoloForRarity(const std::string& rarity);
    void refreshVariantNextControls();
    [[nodiscard]] std::size_t findAvailableSetIndex(const std::string& setId) const;

    EditMode                       dialogMode_;
    unsigned                       variantFetchEpoch_{0};
    CardPreviewService&            cardPreview_;
    std::shared_ptr<VariantFetchState> variantFetchState_;

    wxTextCtrl*  setNoCtrl_{nullptr};
    wxButton*    autoSetNoBtn_{nullptr};
    wxButton*    nextSetNoBtn_{nullptr};
    wxChoice*    rarityChoice_{nullptr};
    wxButton*    autoRarityBtn_{nullptr};
    wxCheckBox*  holoCheck_{nullptr};
    wxCheckBox*  signedCheck_{nullptr};
    wxCheckBox*  alteredCheck_{nullptr};

    std::vector<AutoDetectedPrint> cachedVariants_;
    std::size_t                    variantRingPos_{0};
};

}  // namespace ccm::ui
