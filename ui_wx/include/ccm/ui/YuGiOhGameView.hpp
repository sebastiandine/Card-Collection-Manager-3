#pragma once

#include "ccm/domain/YuGiOhCard.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/services/CollectionService.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/services/SetService.hpp"
#include "ccm/services/YuGiOhSetCatalogService.hpp"
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

class YuGiOhCardListPanel;
class YuGiOhSelectedCardPanel;
class YuGiOhSetCompletionPanel;

class YuGiOhGameView final : public IGameView {
public:
    YuGiOhGameView(ConfigService&                 config,
                   CollectionService<YuGiOhCard>& collection,
                   SetService&                    sets,
                   ImageService&                  images,
                   CardPreviewService&            cardPreview,
                   IGameModule&                   module,
                   YuGiOhSetCatalogService&       catalogStore);

    [[nodiscard]] Game        gameId() const noexcept override { return Game::YuGiOh; }
    [[nodiscard]] std::string displayName() const override { return "Yu-Gi-Oh!"; }

    wxPanel* listPanel(wxWindow* parent) override;
    wxPanel* selectedPanel(wxWindow* parent) override;
    wxPanel* contentPanel(wxWindow* parent) override;
    [[nodiscard]] wxPanel* contentPanelIfCreated() const noexcept override {
        return contentPanel_;
    }
    [[nodiscard]] bool hostsOwnLayout() const noexcept override { return true; }

    void refreshCollection(std::optional<std::uint32_t> selectId = std::nullopt) override;
    void onAddCard(wxWindow* parentWindow) override;
    void onEditCard(wxWindow* parentWindow) override;
    void onDeleteCard(wxWindow* parentWindow) override;
    std::string onUpdateSets(wxWindow* parentWindow) override;
    void setFilter(std::string_view filter) override;
    void nudgeSelection(int delta) override;
    void applyTheme(const ThemePalette& palette) override;
    [[nodiscard]] std::string updateSetsMenuLabel() const override { return "Update Yu-Gi-Oh!"; }

private:
    void ensureSetsLoaded();
    const std::vector<Set>& setsForDialog();
    void ensureSingleCardsMounted(wxWindow* splitterParent);
    void buildSingleCardsToolbar(wxWindow* parent, wxBoxSizer* pageSizer);
    void buildTabBar(wxWindow* parent, wxBoxSizer* rootSizer);
    void selectTab(int index);
    void refreshToolbarIcons(const ThemePalette& palette);
    void refreshTabBarTheme(const ThemePalette& palette);

    ConfigService&                 config_;
    CollectionService<YuGiOhCard>& collection_;
    SetService&                    sets_;
    ImageService&                  images_;
    CardPreviewService&            cardPreview_;
    IGameModule&                   module_;
    YuGiOhSetCatalogService&       catalogStore_;

    wxPanel*                    contentPanel_{nullptr};
    wxPanel*                    tabBar_{nullptr};
    wxSimplebook*               book_{nullptr};
    wxSplitterWindow*           singleSplitter_{nullptr};
    YuGiOhCardListPanel*        listPanel_{nullptr};
    YuGiOhSelectedCardPanel*    selectedPanel_{nullptr};
    YuGiOhSetCompletionPanel*   setCompletionPanel_{nullptr};
    std::array<wxPanel*, 2>     tabPanels_{{nullptr, nullptr}};
    std::array<wxStaticText*, 2> tabLabels_{{nullptr, nullptr}};
    int                         activeTab_{0};
    std::array<wxBitmapButton*, 3> toolbarButtons_{{nullptr, nullptr, nullptr}};
    wxTextCtrl*                 filterInput_{nullptr};
    std::vector<Set>            setsCache_;
    bool                        attemptedInitialSetLoad_{false};
};

}  // namespace ccm::ui
