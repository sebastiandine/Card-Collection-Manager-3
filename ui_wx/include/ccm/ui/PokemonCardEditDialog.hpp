#pragma once

// PokemonCardEditDialog: Add/Edit form for a unified West/Asia PokemonCard.
// West/Asia switch drives set lists, language choices, preview APIs, and the
// Asia-only UnnumberedPromo print UX (from the former Japanese dialog).

#include "ccm/domain/PokemonCard.hpp"
#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/ui/BaseCardEditDialog.hpp"
#include "ccm/ui/SwitchCtrl.hpp"

#include <wx/button.h>
#include <wx/stattext.h>

#include <atomic>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace ccm::ui {

class VariantImagePreviewDialog;

class PokemonCardEditDialog final : public BaseCardEditDialog<PokemonCard> {
public:
    PokemonCardEditDialog(wxWindow* parent,
                          ImageService& imageService,
                          SetService& setService,
                          CardPreviewService& cardPreview,
                          EditMode mode,
                          PokemonCard initial,
                          const std::vector<Set>* westSets = nullptr,
                          const std::vector<Set>* asiaSets = nullptr);
    ~PokemonCardEditDialog() override;

protected:
    void appendPreSetRows(wxFlexGridSizer* grid) override;
    void buildFlagsRow(wxBoxSizer* flagsBox) override;
    void appendExtraRows(wxFlexGridSizer* grid) override;
    void readExtraFromCard() override;
    void writeExtraToCard() override;
    [[nodiscard]] std::string updateMenuName() const override { return "Update Pokemon"; }
    void onCardLookupContextChanged() override;
    [[nodiscard]] std::span<const Language> languagesForChoice() const override;

private:
    struct VariantFetchState {
        std::atomic<bool> alive{true};
    };

    void onRegionSwitch(wxCommandEvent&);
    void applyRegion(PokemonRegion region, bool clearSetIfMissing);
    void onAutoDetectSetNo(wxCommandEvent&);
    void onNextSetNo(wxCommandEvent&);
    void onSetSelectionChanged(wxCommandEvent&);
    void autoDetectFromApi();
    void clearCachedPrintVariants();
    void requestVariantsAsync(unsigned capturedEpoch,
                              std::string name,
                              std::string setId,
                              bool fillSetNoOnSuccess,
                              bool showFailureDialog);
    void requestBySetNoAsync(unsigned capturedEpoch,
                             std::string setId,
                             std::string setNo,
                             bool showFailureDialog);
    void applyDetectedVariants(unsigned capturedEpoch,
                               Result<std::vector<AutoDetectedPrint>> detected,
                               bool fillSetNoOnSuccess,
                               bool fillNameOnSuccess,
                               bool showFailureDialog);
    void rebuildVariantRingFromCache();
    void syncRingPositionToControls();
    void refreshVariantNextControls();
    void refreshSetNoRowMode();
    void applySelectedSetNo(std::string setNo);
    void stepVariantRing(int delta);
    void scheduleDeferredVariantPrefetch();
    void prefetchVariantsForCurrentCardSilent(unsigned capturedEpoch);
    void closeUnnumberedPreview();
    void ensureUnnumberedPreviewOpen();
    void refreshUnnumberedPreview();
    void requestUnnumberedPreviewAsync(unsigned capturedEpoch,
                                       std::string name,
                                       std::string setId,
                                       std::string setNo,
                                       std::size_t ringIndex,
                                       std::size_t ringCount);
    [[nodiscard]] bool isUnnumberedPromoSelected() const;
    [[nodiscard]] std::string currentRingSetNo() const;
    [[nodiscard]] Game backendGame() const noexcept;
    [[nodiscard]] PokemonRegion currentRegion() const noexcept;
    [[nodiscard]] static std::string storedSetNoFromControls(const wxTextCtrl* ctrl);
    [[nodiscard]] static std::string normalizedStoredSetNo(std::string_view setNo);

    EditMode                      dialogMode_;
    unsigned                      variantFetchEpoch_{0};
    unsigned                      previewFetchEpoch_{0};
    CardPreviewService&           cardPreview_;
    const std::vector<Set>*       westSets_{nullptr};
    const std::vector<Set>*       asiaSets_{nullptr};
    std::shared_ptr<VariantFetchState> variantFetchState_;

    SwitchCtrl*                   regionSwitch_{nullptr};
    wxStaticText*                 setNoLabel_{nullptr};
    wxTextCtrl*                   setNoCtrl_{nullptr};
    wxButton*                     autoSetNoBtn_{nullptr};
    wxButton*                     nextSetNoBtn_{nullptr};
    wxCheckBox*                   holoCheck_{nullptr};
    wxCheckBox*                   firstEditionCheck_{nullptr};
    wxCheckBox*                   signedCheck_{nullptr};
    wxCheckBox*                   alteredCheck_{nullptr};
    VariantImagePreviewDialog*    unnumberedPreview_{nullptr};

    std::string                    selectedSetNo_;
    std::vector<AutoDetectedPrint> cachedVariants_;
    std::vector<std::string>       uniqueSetNos_;
    std::size_t                    setNoRingPos_{0};
};

}  // namespace ccm::ui
