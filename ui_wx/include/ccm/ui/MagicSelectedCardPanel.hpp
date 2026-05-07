#pragma once

// MagicSelectedCardPanel: typed view of the right-hand-side detail panel for
// Magic. Inherits the preview-fetch / detail-grid / image-list machinery from
// `BaseSelectedCardPanel<MagicCard>` and only overrides the per-game hooks.

#include "ccm/domain/MagicCard.hpp"
#include "ccm/ui/BaseSelectedCardPanel.hpp"

namespace ccm::ui {

class MagicSelectedCardPanel final : public BaseSelectedCardPanel<MagicCard> {
public:
    MagicSelectedCardPanel(wxWindow* parent,
                           ImageService& imageService,
                           CardPreviewService& cardPreview);

protected:
    [[nodiscard]] std::vector<DetailRowSpec> declareDetailRows() const override;
    [[nodiscard]] std::vector<FlagIconSpec>  declareFlagIcons()  const override;
    [[nodiscard]] std::string detailValueFor(const MagicCard& card, DetailKey key) const override;
    [[nodiscard]] bool        isFlagSet(const MagicCard& card, DetailKey key) const override;
    [[nodiscard]] std::tuple<std::string, std::string, std::string>
        previewKey(const MagicCard& card) const override;
    [[nodiscard]] Game gameId() const noexcept override { return Game::Magic; }
};

}  // namespace ccm::ui
