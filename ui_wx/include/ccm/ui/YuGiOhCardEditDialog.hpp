#pragma once

#include "ccm/domain/YuGiOhCard.hpp"
#include "ccm/ui/BaseCardEditDialog.hpp"

namespace ccm::ui {

class YuGiOhCardEditDialog final : public BaseCardEditDialog<YuGiOhCard> {
public:
    YuGiOhCardEditDialog(wxWindow* parent,
                         ImageService& imageService,
                         SetService& setService,
                         EditMode mode,
                         YuGiOhCard initial,
                         const std::vector<Set>* preloadedSets = nullptr);

protected:
    void buildFlagsRow(wxBoxSizer* flagsBox) override;
    void appendExtraRows(wxFlexGridSizer* grid) override;
    void readExtraFromCard() override;
    void writeExtraToCard() override;
    [[nodiscard]] std::string updateMenuName() const override { return "Update Yu-Gi-Oh!"; }

private:
    wxTextCtrl* setNoCtrl_{nullptr};
    wxChoice*   rarityChoice_{nullptr};
    wxCheckBox* firstEditionCheck_{nullptr};
    wxCheckBox* signedCheck_{nullptr};
    wxCheckBox* alteredCheck_{nullptr};
};

}  // namespace ccm::ui
