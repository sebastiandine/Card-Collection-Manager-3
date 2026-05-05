#pragma once

// MainFrame: top-level window. Hosts the menu bar (File / Game / Sets) and
// the Magic card list + selected card panel. Pokemon menu items are wired up
// but disabled for the MVP (see PokemonGameModule stub).

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/ui/AppContext.hpp"

#include <array>
#include <vector>

#include <wx/frame.h>

class wxTextCtrl;
class wxBitmapButton;
class wxStaticText;
class wxPanel;

namespace ccm::ui {

class MagicCardListPanel;
class SelectedCardPanel;

class MainFrame : public wxFrame {
public:
    explicit MainFrame(AppContext& ctx);

private:
    void buildMenuBar();
    void buildLayout();
    void applyTheme();
    void refreshToolbarIcons();
    void setStatusTextUi(const wxString& text);
    void onOpenFileMenu();
    void onOpenGameMenu();
    void onOpenSetsMenu();
    void switchGame(Game g);
    void refreshCollection();

    void onSettings(wxCommandEvent&);
    void onQuit(wxCommandEvent&);
    void onSwitchMagic(wxCommandEvent&);
    void onSwitchPokemon(wxCommandEvent&);
    void onUpdateSetsMagic(wxCommandEvent&);
    void onUpdateSetsPokemon(wxCommandEvent&);

    void onCreate(wxCommandEvent&);
    void onEdit(wxCommandEvent&);
    void onDelete(wxCommandEvent&);
    const std::vector<Set>& magicSetsForDialog();
#ifdef __WXMSW__
    WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

    AppContext& ctx_;
    Game        activeGame_{Game::Magic};

    MagicCardListPanel* magicList_{nullptr};
    SelectedCardPanel*  selectedPanel_{nullptr};
    wxTextCtrl*         filterInput_{nullptr};
    wxPanel*            menuStrip_{nullptr};
    wxStaticText*       statusText_{nullptr};
    std::array<wxBitmapButton*, 3> toolbarButtons_{{nullptr, nullptr, nullptr}};
    std::vector<Set>    magicSetsCache_;

    enum Ids : int {
        IdSettings = wxID_HIGHEST + 1,
        IdSwitchMagic,
        IdSwitchPokemon,
        IdUpdateSetsMagic,
        IdUpdateSetsPokemon,
        IdCreate,
        IdEdit,
        IdDelete,
        IdMenuFile,
        IdMenuGame,
        IdMenuSets,
    };
};

}  // namespace ccm::ui
