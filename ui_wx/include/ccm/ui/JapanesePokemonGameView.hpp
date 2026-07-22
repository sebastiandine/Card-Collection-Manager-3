#pragma once

// JapanesePokemonGameView: IGameView for the Japanese Pokemon TCG. Mirrors `MagicGameView` —
// owns the Pokemon-typed list, selected, and edit-dialog widgets and
// delegates persistence to a `CollectionService<JapanesePokemonCard>` reference
// supplied by the composition root.

#include "ccm/domain/JapanesePokemonCard.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/services/CollectionService.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/services/SetService.hpp"
#include "ccm/ui/IGameView.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ccm::ui {

class JapanesePokemonCardListPanel;
class JapanesePokemonSelectedCardPanel;

class JapanesePokemonGameView final : public IGameView {
public:
    JapanesePokemonGameView(ConfigService&                         config,
                    CollectionService<JapanesePokemonCard>&        collection,
                    SetService&                            sets,
                    ImageService&                          images,
                    CardPreviewService&                    cardPreview,
                    IGameModule&                           module);

    [[nodiscard]] Game        gameId() const noexcept override { return Game::JapanesePokemon; }
    [[nodiscard]] std::string displayName() const override { return "Pokemon (Japan)"; }

    wxPanel* listPanel(wxWindow* parent) override;
    wxPanel* selectedPanel(wxWindow* parent) override;

    void refreshCollection() override;
    void onAddCard(wxWindow* parentWindow) override;
    void onEditCard(wxWindow* parentWindow) override;
    void onDeleteCard(wxWindow* parentWindow) override;
    std::string onUpdateSets(wxWindow* parentWindow) override;
    void setFilter(std::string_view filter) override;
    void applyTheme(const ThemePalette& palette) override;
    [[nodiscard]] std::string updateSetsMenuLabel() const override { return "Update Pokemon (Japan)"; }

private:
    void ensureSetsLoaded();
    const std::vector<Set>& setsForDialog();

    ConfigService&                  config_;
    CollectionService<JapanesePokemonCard>& collection_;
    SetService&                     sets_;
    ImageService&                   images_;
    CardPreviewService&             cardPreview_;
    IGameModule&                    module_;

    JapanesePokemonCardListPanel*     listPanel_{nullptr};
    JapanesePokemonSelectedCardPanel* selectedPanel_{nullptr};
    std::vector<Set>          setsCache_;
    bool                      attemptedInitialSetLoad_{false};
};

}  // namespace ccm::ui
