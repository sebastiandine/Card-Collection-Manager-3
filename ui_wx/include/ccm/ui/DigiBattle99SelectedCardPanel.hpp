#pragma once

#include "ccm/domain/DigiBattle99Card.hpp"
#include "ccm/ui/BaseSelectedCardPanel.hpp"

namespace ccm::ui {

class DigiBattle99SelectedCardPanel final : public BaseSelectedCardPanel<DigiBattle99Card> {
public:
    DigiBattle99SelectedCardPanel(wxWindow* parent,
                                  ImageService& imageService,
                                  CardPreviewService& cardPreview);

protected:
    [[nodiscard]] std::vector<DetailRowSpec> declareDetailRows() const override;
    [[nodiscard]] std::vector<FlagIconSpec>  declareFlagIcons()  const override;
    [[nodiscard]] std::string detailValueFor(const DigiBattle99Card& card,
                                             DetailKey key) const override;
    [[nodiscard]] bool isFlagSet(const DigiBattle99Card& card, DetailKey key) const override;
    [[nodiscard]] std::tuple<std::string, std::string, std::string>
        previewKey(const DigiBattle99Card& card) const override;
    [[nodiscard]] Game gameId() const noexcept override { return Game::DigiBattle99; }
};

}  // namespace ccm::ui
