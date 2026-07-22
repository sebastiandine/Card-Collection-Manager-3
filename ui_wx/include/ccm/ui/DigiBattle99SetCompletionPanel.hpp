#pragma once

// DigiBattle99SetCompletionPanel: Set Completion tab — pack tiles with
// progress bars for sets the user owns ≥1 card of, plus an in-tab checklist
// drill-down (unowned rows greyed). Catalog is offline (set-catalog.json).

#include "ccm/domain/DigiBattle99Card.hpp"
#include "ccm/domain/DigiBattle99SetCatalog.hpp"
#include "ccm/services/DigiBattle99SetCatalogService.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/panel.h>

#include <string>
#include <vector>

class wxBoxSizer;
class wxListCtrl;
class wxScrolledWindow;
class wxSimplebook;
class wxStaticText;

namespace ccm::ui {

class DigiBattle99SetCompletionPanel : public wxPanel {
public:
    DigiBattle99SetCompletionPanel(wxWindow* parent, DigiBattle99SetCatalogService& catalogStore);

    void setCollection(std::vector<DigiBattle99Card> cards);
    void reloadFromStore();
    void applyTheme(const ThemePalette& palette);

private:
    void showGridPage();
    void showChecklistPage(const std::string& setId, const std::string& setName);
    void rebuildGrid();
    void rebuildChecklist(const std::string& setId);
    void setEmptyMessage(const wxString& message);
    void clearGridTiles();

    DigiBattle99SetCatalogService&     catalogStore_;
    DigiBattle99SetCatalog             catalog_;
    bool                               catalogLoaded_{false};
    std::vector<DigiBattle99Card>      collection_;
    ThemePalette                       palette_{};

    wxSimplebook*      book_{nullptr};
    wxPanel*           gridPage_{nullptr};
    wxScrolledWindow*  scroll_{nullptr};
    wxBoxSizer*        gridSizer_{nullptr};
    wxStaticText*      emptyLabel_{nullptr};

    wxPanel*           detailPage_{nullptr};
    wxStaticText*      detailTitle_{nullptr};
    wxListCtrl*        checklist_{nullptr};
    std::string        detailSetId_;
};

}  // namespace ccm::ui
