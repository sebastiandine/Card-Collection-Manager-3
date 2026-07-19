#pragma once

#include "ccm/domain/DigiBattle99Card.hpp"
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

class DigiBattle99CardListPanel;
class DigiBattle99SelectedCardPanel;

class DigiBattle99GameView final : public IGameView {
public:
    DigiBattle99GameView(ConfigService&                         config,
                         CollectionService<DigiBattle99Card>&   collection,
                         SetService&                            sets,
                         ImageService&                          images,
                         CardPreviewService&                    cardPreview,
                         IGameModule&                           module);

    [[nodiscard]] Game        gameId() const noexcept override { return Game::DigiBattle99; }
    [[nodiscard]] std::string displayName() const override { return "Digimon (Digi-Battle)"; }

    wxPanel* listPanel(wxWindow* parent) override;
    wxPanel* selectedPanel(wxWindow* parent) override;

    void refreshCollection() override;
    void onAddCard(wxWindow* parentWindow) override;
    void onEditCard(wxWindow* parentWindow) override;
    void onDeleteCard(wxWindow* parentWindow) override;
    std::string onUpdateSets(wxWindow* parentWindow) override;
    void setFilter(std::string_view filter) override;
    void applyTheme(const ThemePalette& palette) override;
    [[nodiscard]] std::string updateSetsMenuLabel() const override {
        return "Update Digimon (Digi-Battle)";
    }

private:
    void ensureSetsLoaded();
    const std::vector<Set>& setsForDialog();

    ConfigService&                        config_;
    CollectionService<DigiBattle99Card>&  collection_;
    SetService&                           sets_;
    ImageService&                         images_;
    CardPreviewService&                   cardPreview_;
    IGameModule&                          module_;

    DigiBattle99CardListPanel*     listPanel_{nullptr};
    DigiBattle99SelectedCardPanel* selectedPanel_{nullptr};
    std::vector<Set>               setsCache_;
    bool                           attemptedInitialSetLoad_{false};
};

}  // namespace ccm::ui
