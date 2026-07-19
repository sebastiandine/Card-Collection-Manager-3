#pragma once

#include "ccm/domain/DigiBattle99Card.hpp"
#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/ui/BaseCardEditDialog.hpp"
#include <wx/button.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace ccm::ui {

class DigiBattle99CardEditDialog final : public BaseCardEditDialog<DigiBattle99Card> {
public:
    DigiBattle99CardEditDialog(wxWindow* parent,
                               ImageService& imageService,
                               SetService& setService,
                               CardPreviewService& cardPreview,
                               EditMode mode,
                               DigiBattle99Card initial,
                               const std::vector<Set>* preloadedSets = nullptr);
    ~DigiBattle99CardEditDialog() override;

protected:
    void buildFlagsRow(wxBoxSizer* flagsBox) override;
    void appendExtraRows(wxFlexGridSizer* grid) override;
    void readExtraFromCard() override;
    void writeExtraToCard() override;
    [[nodiscard]] std::string updateMenuName() const override {
        return "Update Digimon (Digi-Battle)";
    }
    void onCardLookupContextChanged() override;

private:
    struct VariantFetchState {
        std::atomic<bool> alive{true};
    };

    void onAutoDetectSetNo(wxCommandEvent&);
    void onNextSetNo(wxCommandEvent&);
    void onSetSelectionChanged(wxCommandEvent&);
    void autoDetectFromApi();
    void clearCachedPrintVariants();
    void requestVariantsAsync(unsigned capturedEpoch,
                              std::string name,
                              std::string setName,
                              bool fillSetNoOnSuccess,
                              bool showFailureDialog);
    void applyDetectedVariants(unsigned capturedEpoch,
                               Result<std::vector<AutoDetectedPrint>> detected,
                               bool fillSetNoOnSuccess,
                               bool showFailureDialog);
    void rebuildVariantRingFromCache();
    void syncRingPositionToControls();
    void refreshVariantNextControls();
    void scheduleDeferredVariantPrefetch();
    void prefetchVariantsForCurrentCardSilent(unsigned capturedEpoch);
    [[nodiscard]] static std::string storedSetNoFromControls(const wxTextCtrl* ctrl);
    [[nodiscard]] static std::string normalizedStoredSetNo(std::string_view setNo);

    EditMode                      dialogMode_;
    unsigned                      variantFetchEpoch_{0};
    CardPreviewService&           cardPreview_;
    std::shared_ptr<VariantFetchState> variantFetchState_;
    wxTextCtrl*                   setNoCtrl_{nullptr};
    wxButton*                     autoSetNoBtn_{nullptr};
    wxButton*                     nextSetNoBtn_{nullptr};
    wxCheckBox*                   holoCheck_{nullptr};
    wxCheckBox*                   firstEditionCheck_{nullptr};
    wxCheckBox*                   signedCheck_{nullptr};
    wxCheckBox*                   alteredCheck_{nullptr};

    std::vector<AutoDetectedPrint> cachedVariants_;
    std::vector<std::string>       uniqueSetNos_;
    std::size_t                    setNoRingPos_{0};
};

}  // namespace ccm::ui
