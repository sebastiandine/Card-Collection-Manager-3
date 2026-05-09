#pragma once

#include "ccm/domain/YuGiOhCard.hpp"
#include "ccm/ui/BaseSelectedCardPanel.hpp"

namespace ccm::ui {

class YuGiOhSelectedCardPanel final : public BaseSelectedCardPanel<YuGiOhCard> {
public:
    YuGiOhSelectedCardPanel(wxWindow* parent,
                            ImageService& imageService,
                            CardPreviewService& cardPreview);

protected:
    [[nodiscard]] std::vector<DetailRowSpec> declareDetailRows() const override;
    [[nodiscard]] std::vector<FlagIconSpec>  declareFlagIcons()  const override;
    [[nodiscard]] std::string detailValueFor(const YuGiOhCard& card, DetailKey key) const override;
    [[nodiscard]] bool        isFlagSet(const YuGiOhCard& card, DetailKey key) const override;
    [[nodiscard]] std::tuple<std::string, std::string, std::string>
        previewKey(const YuGiOhCard& card) const override;
    [[nodiscard]] Game gameId() const noexcept override { return Game::YuGiOh; }
};

}  // namespace ccm::ui
