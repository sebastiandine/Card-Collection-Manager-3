#pragma once

// MagicGameView: IGameView for Magic the Gathering. Hosts Single Cards |
// Deck Check via contentPanel / hostsOwnLayout. Owns list, selected, and
// Deck Check panels and delegates persistence to CollectionService<MagicCard>.

#include "ccm/domain/MagicCard.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/services/CollectionService.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/ImageService.hpp"
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

class MagicCardListPanel;
class MagicSelectedCardPanel;
class MagicDeckCheckPanel;

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
    [[nodiscard]] std::string updateSetsMenuLabel() const override { return "Update Magic"; }

private:
    void syncEditToolbarVisibility();
    void ensureSetsLoaded();
    const std::vector<Set>& setsForDialog();
    void ensureSingleCardsMounted(wxWindow* splitterParent);
    void buildSingleCardsToolbar(wxWindow* parent, wxBoxSizer* pageSizer);
    void buildTabBar(wxWindow* parent, wxBoxSizer* rootSizer);
    void selectTab(int index);
    void refreshToolbarIcons(const ThemePalette& palette);
    void refreshTabBarTheme(const ThemePalette& palette);

    ConfigService&                config_;
    CollectionService<MagicCard>& collection_;
    SetService&                   sets_;
    ImageService&                 images_;
    CardPreviewService&           cardPreview_;
    IGameModule&                  module_;

    wxPanel*                    contentPanel_{nullptr};
    wxPanel*                    tabBar_{nullptr};
    wxSimplebook*               book_{nullptr};
    wxSplitterWindow*           singleSplitter_{nullptr};
    MagicCardListPanel*         listPanel_{nullptr};
    MagicSelectedCardPanel*     selectedPanel_{nullptr};
    MagicDeckCheckPanel*        deckCheckPanel_{nullptr};
    std::array<wxPanel*, 2>     tabPanels_{{nullptr, nullptr}};
    std::array<wxStaticText*, 2> tabLabels_{{nullptr, nullptr}};
    int                         activeTab_{0};
    std::array<wxBitmapButton*, 3> toolbarButtons_{{nullptr, nullptr, nullptr}};
    wxTextCtrl*                 filterInput_{nullptr};
    std::vector<Set>            setsCache_;
    bool                        attemptedInitialSetLoad_{false};
};

}  // namespace ccm::ui
