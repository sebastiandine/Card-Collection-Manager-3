#pragma once

#include "ccm/domain/YuGiOhBandaiCard.hpp"
#include "ccm/ui/BaseSelectedCardPanel.hpp"

namespace ccm::ui {

class YuGiOhBandaiSelectedCardPanel final : public BaseSelectedCardPanel<YuGiOhBandaiCard> {
public:
    YuGiOhBandaiSelectedCardPanel(wxWindow* parent,
                                  ImageService& imageService,
                                  CardPreviewService& cardPreview);

protected:
    [[nodiscard]] std::vector<DetailRowSpec> declareDetailRows() const override;
    [[nodiscard]] std::vector<FlagIconSpec>  declareFlagIcons()  const override;
    [[nodiscard]] std::string detailValueFor(const YuGiOhBandaiCard& card,
                                             DetailKey key) const override;
    [[nodiscard]] bool isFlagSet(const YuGiOhBandaiCard& card, DetailKey key) const override;
    [[nodiscard]] std::tuple<std::string, std::string, std::string>
        previewKey(const YuGiOhBandaiCard& card) const override;
    [[nodiscard]] Game gameId() const noexcept override { return Game::YuGiOhBandai; }
};

}  // namespace ccm::ui
