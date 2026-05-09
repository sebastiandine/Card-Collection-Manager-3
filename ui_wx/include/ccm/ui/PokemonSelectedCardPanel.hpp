#pragma once

// PokemonSelectedCardPanel: typed view of the right-hand-side detail panel
// for Pokemon TCG cards. Inherits from `BaseSelectedCardPanel<PokemonCard>`
// and only overrides per-game hooks (detail rows now include `Set #`,
// flag strip is `Holo` / `1. Ed` / `Signed` / `Altered`, preview lookup
// includes the collector number).

#include "ccm/domain/PokemonCard.hpp"
#include "ccm/ui/BaseSelectedCardPanel.hpp"

namespace ccm::ui {

class PokemonSelectedCardPanel final : public BaseSelectedCardPanel<PokemonCard> {
public:
    PokemonSelectedCardPanel(wxWindow* parent,
                             ImageService& imageService,
                             CardPreviewService& cardPreview);

protected:
    [[nodiscard]] std::vector<DetailRowSpec> declareDetailRows() const override;
    [[nodiscard]] std::vector<FlagIconSpec>  declareFlagIcons()  const override;
    [[nodiscard]] std::string detailValueFor(const PokemonCard& card, DetailKey key) const override;
    [[nodiscard]] bool        isFlagSet(const PokemonCard& card, DetailKey key) const override;
    [[nodiscard]] std::tuple<std::string, std::string, std::string>
        previewKey(const PokemonCard& card) const override;
    [[nodiscard]] Game gameId() const noexcept override { return Game::Pokemon; }
};

}  // namespace ccm::ui
