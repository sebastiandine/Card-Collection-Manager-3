#pragma once

// PokemonCardEditDialog: typed Add/Edit form for a `PokemonCard`. Inherits
// the shared layout, set picker, and image management from
// `BaseCardEditDialog<PokemonCard>` and adds:
//   - a `Set #` text input (between the Set picker and the Amount spin)
//   - `Holo`, `1. Edition`, `Signed`, `Altered` check boxes in the flags row

#include "ccm/domain/PokemonCard.hpp"
#include "ccm/ui/BaseCardEditDialog.hpp"

namespace ccm::ui {

class PokemonCardEditDialog final : public BaseCardEditDialog<PokemonCard> {
public:
    PokemonCardEditDialog(wxWindow* parent,
                          ImageService& imageService,
                          SetService& setService,
                          EditMode mode,
                          PokemonCard initial,
                          const std::vector<Set>* preloadedSets = nullptr);

protected:
    void buildFlagsRow(wxBoxSizer* flagsBox) override;
    void appendExtraRows(wxFlexGridSizer* grid) override;
    void readExtraFromCard() override;
    void writeExtraToCard() override;
    [[nodiscard]] std::string updateMenuName() const override { return "Update Pokemon"; }

private:
    wxTextCtrl* setNoCtrl_{nullptr};
    wxCheckBox* holoCheck_{nullptr};
    wxCheckBox* firstEditionCheck_{nullptr};
    wxCheckBox* signedCheck_{nullptr};
    wxCheckBox* alteredCheck_{nullptr};
};

}  // namespace ccm::ui
