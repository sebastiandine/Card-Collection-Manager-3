#pragma once

// MagicGameView: IGameView for Magic the Gathering. Owns its three panels
// (list, selected, edit-dialog state) and delegates persistence to the
// typed `CollectionService<MagicCard>` reference handed in by the
// composition root.

#include "ccm/domain/MagicCard.hpp"
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

class MagicCardListPanel;
class MagicSelectedCardPanel;

class MagicGameView final : public IGameView {
public:
    MagicGameView(ConfigService&                       config,
                  CollectionService<MagicCard>&        collection,
                  SetService&                          sets,
                  ImageService&                        images,
                  CardPreviewService&                  cardPreview,
                  IGameModule&                         module);

    [[nodiscard]] Game        gameId() const noexcept override { return Game::Magic; }
    [[nodiscard]] std::string displayName() const override { return "Magic"; }

    wxPanel* listPanel(wxWindow* parent) override;
    wxPanel* selectedPanel(wxWindow* parent) override;

    void refreshCollection() override;
    void onAddCard(wxWindow* parentWindow) override;
    void onEditCard(wxWindow* parentWindow) override;
    void onDeleteCard(wxWindow* parentWindow) override;
    std::string onUpdateSets(wxWindow* parentWindow) override;
    void setFilter(std::string_view filter) override;
    void applyTheme(const ThemePalette& palette) override;
    [[nodiscard]] std::string updateSetsMenuLabel() const override { return "Update Magic"; }

private:
    void ensureSetsLoaded();
    const std::vector<Set>& setsForDialog();

    ConfigService&                config_;
    CollectionService<MagicCard>& collection_;
    SetService&                   sets_;
    ImageService&                 images_;
    CardPreviewService&           cardPreview_;
    IGameModule&                  module_;

    MagicCardListPanel*     listPanel_{nullptr};
    MagicSelectedCardPanel* selectedPanel_{nullptr};
    std::vector<Set>        setsCache_;
    bool                    attemptedInitialSetLoad_{false};
};

}  // namespace ccm::ui
