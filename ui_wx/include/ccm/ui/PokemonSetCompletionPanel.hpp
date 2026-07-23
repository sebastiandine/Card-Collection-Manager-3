#pragma once

// PokemonSetCompletionPanel: Set Completion tab — pack tiles with progress
// bars for sets the user owns ≥1 card of, plus an in-tab checklist drill-down
// (unowned rows greyed). Dual offline catalogs (West + Asia). Optional region
// and language filters restrict ownership; set titles may be annotated with
// region and/or language.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/PokemonCard.hpp"
#include "ccm/domain/PokemonSetCatalog.hpp"
#include "ccm/services/PokemonSetCatalogService.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/panel.h>

#include <optional>
#include <string>
#include <vector>

class wxBoxSizer;
class wxChoice;
class wxListCtrl;
class wxScrolledWindow;
class wxSimplebook;
class wxStaticText;

namespace ccm::ui {

class PokemonSetCompletionPanel : public wxPanel {
public:
    PokemonSetCompletionPanel(wxWindow* parent, PokemonSetCatalogService& catalogStore);

    void setCollection(std::vector<PokemonCard> cards);
    void reloadFromStore();
    void applyTheme(const ThemePalette& palette);

private:
    void showGridPage();
    void showChecklistPage(PokemonRegion region, const std::string& setId,
                           const std::string& setName);
    void rebuildGrid();
    void rebuildChecklist(PokemonRegion region, const std::string& setId);
    void setEmptyMessage(const wxString& message);
    void clearGridTiles();
    void refreshRegionChoice();
    void refreshLanguageChoice();
    void onRegionChoice(wxCommandEvent& event);
    void onLanguageChoice(wxCommandEvent& event);
    void rebuildCurrentView();
    [[nodiscard]] std::string displaySetName(const std::string& setName,
                                             PokemonRegion      region) const;
    [[nodiscard]] bool catalogsReadyForFilter() const;

    PokemonSetCatalogService& catalogStore_;
    PokemonSetCatalog         westCatalog_;
    PokemonSetCatalog         asiaCatalog_;
    bool                      westCatalogLoaded_{false};
    bool                      asiaCatalogLoaded_{false};
    std::vector<PokemonCard>  collection_;
    ThemePalette              palette_{};
    std::optional<PokemonRegion> regionFilter_;
    std::optional<Language>      languageFilter_;

    wxChoice*          regionChoice_{nullptr};
    wxChoice*          languageChoice_{nullptr};
    wxSimplebook*      book_{nullptr};
    wxPanel*           gridPage_{nullptr};
    wxScrolledWindow*  scroll_{nullptr};
    wxBoxSizer*        gridSizer_{nullptr};
    wxStaticText*      emptyLabel_{nullptr};

    wxPanel*           detailPage_{nullptr};
    wxStaticText*      detailTitle_{nullptr};
    wxListCtrl*        checklist_{nullptr};
    PokemonRegion      detailRegion_{PokemonRegion::West};
    std::string        detailSetId_;
    std::string        detailSetName_;
};

}  // namespace ccm::ui
