#pragma once

// MainFrame: top-level window. Hosts the menu bar (File / Game / Sets), the
// toolbar (Add / Edit / Delete + filter input), and the splitter that swaps
// the active `IGameView`'s panels in and out as the user switches games.

#include "ccm/domain/Enums.hpp"
#include "ccm/ui/AppContext.hpp"

#include <array>
#include <unordered_map>

#include <wx/frame.h>

class wxTextCtrl;
class wxBitmapButton;
class wxStaticText;
class wxPanel;
class wxSplitterWindow;

namespace ccm::ui {

class IGameView;

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
    void onOpenHelpMenu();
    void switchGame(Game g);
    void mountActiveView();

    void onSettings(wxCommandEvent&);
    void onQuit(wxCommandEvent&);
    void onSwitchGame(wxCommandEvent& ev);
    void onUpdateSetsForGame(wxCommandEvent& ev);
    void onAbout(wxCommandEvent&);

    void onCreate(wxCommandEvent&);
    void onEdit(wxCommandEvent&);
    void onDelete(wxCommandEvent&);

    [[nodiscard]] IGameView* activeView();

#ifdef __WXMSW__
    WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

    AppContext& ctx_;
    Game        activeGame_{Game::Magic};

    wxSplitterWindow*   splitter_{nullptr};
    wxTextCtrl*         filterInput_{nullptr};
    wxPanel*            menuStrip_{nullptr};
    wxStaticText*       statusText_{nullptr};
    std::array<wxBitmapButton*, 3> toolbarButtons_{{nullptr, nullptr, nullptr}};

    // Tracks the dynamic Game / Sets menu item ids for the current popup.
    // We allocate a contiguous block per menu open so the event handler can
    // map back to a `Game` value without a per-game member id.
    std::unordered_map<int, Game> menuIdToGame_;

    enum Ids : int {
        IdSettings = wxID_HIGHEST + 1,
        IdCreate,
        IdEdit,
        IdDelete,
        IdAbout,
        // 8 dynamic ids for game-switch (max 4) and update-sets (max 4) entries.
        IdGameMenuBase,
        IdGameMenuLast    = IdGameMenuBase    + 8,
        IdSetsMenuBase,
        IdSetsMenuLast    = IdSetsMenuBase    + 8,
    };
};

}  // namespace ccm::ui
