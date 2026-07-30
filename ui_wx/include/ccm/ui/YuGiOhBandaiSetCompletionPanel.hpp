#pragma once

// YuGiOhBandaiSetCompletionPanel: Set Completion tab — pack tiles with
// progress bars for sets the user owns >=1 card of, plus an in-tab checklist
// drill-down (unowned rows greyed). Catalog is offline (set-catalog.json).
// Optional language filter restricts ownership to one language and labels
// set titles as "{setName} ({language})".

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/YuGiOhBandaiCard.hpp"
#include "ccm/domain/YuGiOhBandaiSetCatalog.hpp"
#include "ccm/services/YuGiOhBandaiSetCatalogService.hpp"
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

class YuGiOhBandaiSetCompletionPanel : public wxPanel {
public:
    YuGiOhBandaiSetCompletionPanel(wxWindow* parent, YuGiOhBandaiSetCatalogService& catalogStore);

    void setCollection(std::vector<YuGiOhBandaiCard> cards);
    void reloadFromStore();
    void applyTheme(const ThemePalette& palette);

private:
    void showGridPage();
    void showChecklistPage(const std::string& setId, const std::string& setName);
    void rebuildGrid();
    void rebuildChecklist(const std::string& setId);
    void setEmptyMessage(const wxString& message);
    void clearGridTiles();
    void refreshLanguageChoice();
    void onLanguageChoice(wxCommandEvent& event);
    void rebuildCurrentView();
    [[nodiscard]] std::string displaySetName(const std::string& setName) const;

    YuGiOhBandaiSetCatalogService&     catalogStore_;
    YuGiOhBandaiSetCatalog             catalog_;
    bool                               catalogLoaded_{false};
    std::vector<YuGiOhBandaiCard>      collection_;
    ThemePalette                       palette_{};
    std::optional<Language>            languageFilter_;

    wxChoice*          languageChoice_{nullptr};
    wxSimplebook*      book_{nullptr};
    wxPanel*           gridPage_{nullptr};
    wxScrolledWindow*  scroll_{nullptr};
    wxBoxSizer*        gridSizer_{nullptr};
    wxStaticText*      emptyLabel_{nullptr};

    wxPanel*           detailPage_{nullptr};
    wxStaticText*      detailTitle_{nullptr};
    wxListCtrl*        checklist_{nullptr};
    std::string        detailSetId_;
    std::string        detailSetName_;
};

}  // namespace ccm::ui
