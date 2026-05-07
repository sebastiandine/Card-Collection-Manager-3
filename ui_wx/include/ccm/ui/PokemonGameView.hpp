#pragma once

// PokemonGameView: IGameView for the Pokemon TCG. Mirrors `MagicGameView` —
// owns the Pokemon-typed list, selected, and edit-dialog widgets and
// delegates persistence to a `CollectionService<PokemonCard>` reference
// supplied by the composition root.

#include "ccm/domain/PokemonCard.hpp"
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

class PokemonCardListPanel;
class PokemonSelectedCardPanel;

class PokemonGameView final : public IGameView {
public:
    PokemonGameView(ConfigService&                         config,
                    CollectionService<PokemonCard>&        collection,
                    SetService&                            sets,
                    ImageService&                          images,
                    CardPreviewService&                    cardPreview,
                    IGameModule&                           module);

    [[nodiscard]] Game        gameId() const noexcept override { return Game::Pokemon; }
    [[nodiscard]] std::string displayName() const override { return "Pokemon"; }

    wxPanel* listPanel(wxWindow* parent) override;
    wxPanel* selectedPanel(wxWindow* parent) override;

    void refreshCollection() override;
    void onAddCard(wxWindow* parentWindow) override;
    void onEditCard(wxWindow* parentWindow) override;
    void onDeleteCard(wxWindow* parentWindow) override;
    std::string onUpdateSets(wxWindow* parentWindow) override;
    void setFilter(std::string_view filter) override;
    void applyTheme(const ThemePalette& palette) override;
    [[nodiscard]] std::string updateSetsMenuLabel() const override { return "Update Pokemon"; }

private:
    void ensureSetsLoaded();
    const std::vector<Set>& setsForDialog();

    ConfigService&                  config_;
    CollectionService<PokemonCard>& collection_;
    SetService&                     sets_;
    ImageService&                   images_;
    CardPreviewService&             cardPreview_;
    IGameModule&                    module_;

    PokemonCardListPanel*     listPanel_{nullptr};
    PokemonSelectedCardPanel* selectedPanel_{nullptr};
    std::vector<Set>          setsCache_;
    bool                      attemptedInitialSetLoad_{false};
};

}  // namespace ccm::ui
