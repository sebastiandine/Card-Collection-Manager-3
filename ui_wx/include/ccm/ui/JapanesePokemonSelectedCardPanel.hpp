#pragma once

// JapanesePokemonSelectedCardPanel: typed view of the right-hand-side detail panel
// for Japanese Pokemon TCG cards. Inherits from `BaseSelectedCardPanel<JapanesePokemonCard>`
// and only overrides per-game hooks (detail rows now include `Set #`,
// flag strip is `Holo` / `1. Ed` / `Signed` / `Altered`, preview lookup
// includes the collector number).

#include "ccm/domain/JapanesePokemonCard.hpp"
#include "ccm/ui/BaseSelectedCardPanel.hpp"

namespace ccm::ui {

class JapanesePokemonSelectedCardPanel final : public BaseSelectedCardPanel<JapanesePokemonCard> {
public:
    JapanesePokemonSelectedCardPanel(wxWindow* parent,
                             ImageService& imageService,
                             CardPreviewService& cardPreview);

protected:
    [[nodiscard]] std::vector<DetailRowSpec> declareDetailRows() const override;
    [[nodiscard]] std::vector<FlagIconSpec>  declareFlagIcons()  const override;
    [[nodiscard]] std::string detailValueFor(const JapanesePokemonCard& card, DetailKey key) const override;
    [[nodiscard]] bool        isFlagSet(const JapanesePokemonCard& card, DetailKey key) const override;
    [[nodiscard]] std::tuple<std::string, std::string, std::string>
        previewKey(const JapanesePokemonCard& card) const override;
    [[nodiscard]] Game gameId() const noexcept override { return Game::JapanesePokemon; }
};

}  // namespace ccm::ui
