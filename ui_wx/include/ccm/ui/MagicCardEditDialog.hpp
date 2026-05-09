#pragma once

// MagicCardEditDialog: typed Add/Edit form for a `MagicCard`. Inherits the
// shared layout, set picker, and image management from
// `BaseCardEditDialog<MagicCard>` and only overrides the flags row.

#include "ccm/domain/MagicCard.hpp"
#include "ccm/ui/BaseCardEditDialog.hpp"

namespace ccm::ui {

class MagicCardEditDialog final : public BaseCardEditDialog<MagicCard> {
public:
    MagicCardEditDialog(wxWindow* parent,
                        ImageService& imageService,
                        SetService& setService,
                        EditMode mode,
                        MagicCard initial,
                        const std::vector<Set>* preloadedSets = nullptr);

protected:
    void buildFlagsRow(wxBoxSizer* flagsBox) override;
    void readExtraFromCard() override;
    void writeExtraToCard() override;
    [[nodiscard]] std::string updateMenuName() const override { return "Update Magic"; }

private:
    wxCheckBox* foilCheck_{nullptr};
    wxCheckBox* signedCheck_{nullptr};
    wxCheckBox* alteredCheck_{nullptr};
};

}  // namespace ccm::ui
