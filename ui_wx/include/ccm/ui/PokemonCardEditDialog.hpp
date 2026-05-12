#pragma once

// PokemonCardEditDialog: typed Add/Edit form for a `PokemonCard`. Inherits
// the shared layout, set picker, and image management from
// `BaseCardEditDialog<PokemonCard>` and adds:
//   - a `Set #` text input (between the Set picker and the Amount spin)
//   - `Holo`, `1. Edition`, `Signed`, `Altered` check boxes in the flags row

#include "ccm/domain/PokemonCard.hpp"
#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/ui/BaseCardEditDialog.hpp"
#include <wx/button.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace ccm::ui {

class PokemonCardEditDialog final : public BaseCardEditDialog<PokemonCard> {
public:
    PokemonCardEditDialog(wxWindow* parent,
                          ImageService& imageService,
                          SetService& setService,
                          CardPreviewService& cardPreview,
                          EditMode mode,
                          PokemonCard initial,
                          const std::vector<Set>* preloadedSets = nullptr);
    ~PokemonCardEditDialog() override;

protected:
    void buildFlagsRow(wxBoxSizer* flagsBox) override;
    void appendExtraRows(wxFlexGridSizer* grid) override;
    void readExtraFromCard() override;
    void writeExtraToCard() override;
    [[nodiscard]] std::string updateMenuName() const override { return "Update Pokemon"; }
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
                              std::string setId,
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
