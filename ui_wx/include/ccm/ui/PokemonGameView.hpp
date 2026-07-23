#pragma once

// PokemonGameView: unified West + Asia Pokemon UI. One collection file;
// separate West/Asia set caches and set-completion catalogs. Sets > Update
// Pokemon refreshes both regions. Hosts Single Cards | Set Completion tabs.

#include "ccm/domain/PokemonCard.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/services/CollectionService.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/services/PokemonSetCatalogService.hpp"
#include "ccm/services/SetService.hpp"
#include "ccm/ui/IGameView.hpp"

#include <array>
#include <string>
#include <string_view>
#include <vector>

class wxBitmapButton;
class wxBoxSizer;
class wxPanel;
class wxSimplebook;
class wxSplitterWindow;
class wxStaticText;
class wxTextCtrl;

namespace ccm::ui {

class PokemonCardListPanel;
class PokemonSelectedCardPanel;
class PokemonSetCompletionPanel;

class PokemonGameView final : public IGameView {
public:
    PokemonGameView(ConfigService&                         config,
                    CollectionService<PokemonCard>&        collection,
                    SetService&                            sets,
                    ImageService&                          images,
                    CardPreviewService&                    cardPreview,
                    IGameModule&                           westModule,
                    IGameModule&                           asiaModule,
                    PokemonSetCatalogService&              catalogStore);

    [[nodiscard]] Game        gameId() const noexcept override { return Game::Pokemon; }
    [[nodiscard]] std::string displayName() const override { return "Pokemon"; }

    wxPanel* listPanel(wxWindow* parent) override;
    wxPanel* selectedPanel(wxWindow* parent) override;
    wxPanel* contentPanel(wxWindow* parent) override;
    [[nodiscard]] wxPanel* contentPanelIfCreated() const noexcept override {
        return contentPanel_;
    }
    [[nodiscard]] bool hostsOwnLayout() const noexcept override { return true; }

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
    const std::vector<Set>& setsForDialog(PokemonRegion region);
    void ensureSingleCardsMounted(wxWindow* splitterParent);
    void buildSingleCardsToolbar(wxWindow* parent, wxBoxSizer* pageSizer);
    void buildTabBar(wxWindow* parent, wxBoxSizer* rootSizer);
    void selectTab(int index);
    void refreshToolbarIcons(const ThemePalette& palette);
    void refreshTabBarTheme(const ThemePalette& palette);

    ConfigService&                  config_;
    CollectionService<PokemonCard>& collection_;
    SetService&                     sets_;
    ImageService&                   images_;
    CardPreviewService&             cardPreview_;
    IGameModule&                    westModule_;
    IGameModule&                    asiaModule_;
    PokemonSetCatalogService&       catalogStore_;

    wxPanel*                    contentPanel_{nullptr};
    wxPanel*                    tabBar_{nullptr};
    wxSimplebook*               book_{nullptr};
    wxSplitterWindow*           singleSplitter_{nullptr};
    PokemonCardListPanel*       listPanel_{nullptr};
    PokemonSelectedCardPanel*   selectedPanel_{nullptr};
    PokemonSetCompletionPanel*  setCompletionPanel_{nullptr};
    std::array<wxPanel*, 2>     tabPanels_{{nullptr, nullptr}};
    std::array<wxStaticText*, 2> tabLabels_{{nullptr, nullptr}};
    int                         activeTab_{0};
    std::array<wxBitmapButton*, 3> toolbarButtons_{{nullptr, nullptr, nullptr}};
    wxTextCtrl*                 filterInput_{nullptr};
    std::vector<Set>            setsCacheWest_;
    std::vector<Set>            setsCacheAsia_;
    bool                        attemptedInitialSetLoad_{false};
};

}  // namespace ccm::ui
