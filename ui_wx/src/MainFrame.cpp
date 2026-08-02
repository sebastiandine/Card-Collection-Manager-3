#include "ccm/ui/MainFrame.hpp"

// BaseSelectedCardPanel.hpp is included for the shared EVT_PREVIEW_STATUS
// declaration so MainFrame can subscribe to preview-status updates from any
// active selected panel without depending on a specific game's view.
// BaseCardListPanel.hpp provides EVT_UI_STATUS for list-driven status notes
// (e.g. clipboard copy).
#include "ccm/ui/BaseCardListPanel.hpp"
#include "ccm/ui/BaseSelectedCardPanel.hpp"
#include "ccm/ui/IGameView.hpp"
#include "ccm/ui/AppVersion.hpp"
#include "ccm/ui/SettingsDialog.hpp"
#include "ccm/ui/SvgIcons.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/bmpbuttn.h>
#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/menu.h>
#include <wx/menuitem.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

namespace ccm::ui {

namespace {
constexpr int kToolbarIconPx = 18;
constexpr const char kFilterInputHint[] = "Filter";

std::string dirNameForGame(Game g) {
    switch (g) {
        case Game::Magic:           return "magic";
        case Game::Pokemon:         return "pokemon";
        case Game::YuGiOh:          return "yugioh";
        case Game::YuGiOhBandai:    return "yugiohbandai";
        case Game::DigiBattle99:    return "digibattle99";
        case Game::JapanesePokemon: return "pokemon";
    }
    return "magic";
}

void ensureDataStorageScaffold(const Configuration& cfg) {
    namespace fs = std::filesystem;
    const fs::path root(cfg.dataStorage);
    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) return;

    const fs::path dataConfigPath = root / "config.json";
    if (!fs::exists(dataConfigPath, ec)) {
        std::ofstream out(dataConfigPath.string(), std::ios::out | std::ios::trunc);
        if (out.is_open()) {
            // Marker file so moved data folders are self-contained on disk.
            out << "{\n"
                << "  \"dataStorage\": \"" << cfg.dataStorage << "\",\n"
                << "  \"defaultGame\": \"" << to_string(cfg.defaultGame) << "\",\n"
                << "  \"theme\": \"" << to_string(cfg.theme) << "\"\n"
                << "}\n";
        }
    }

    for (Game game : allGames()) {
        const fs::path gameRoot = root / dirNameForGame(game);
        fs::create_directories(gameRoot / "images", ec);
        if (ec) continue;

        const fs::path collectionPath = gameRoot / "collection.json";
        if (!fs::exists(collectionPath, ec)) {
            std::ofstream out(collectionPath.string(), std::ios::out | std::ios::trunc);
            if (out.is_open()) out << "{}\n";
        }
    }
}
}  // namespace

MainFrame::MainFrame(AppContext& ctx)
    : wxFrame(nullptr, wxID_ANY, "Card Collection Manager 3",
              wxDefaultPosition, wxSize(1210, 770)),
      ctx_(ctx),
      activeGame_(ctx.config.current().defaultGame) {
    buildMenuBar();
    buildLayout();
    applyTheme();
    setStatusTextUi("Ready");

    setStatusTextUi("Loading collection...");
    CallAfter([this]() {
        mountActiveView();
        if (auto* view = activeView()) view->refreshCollection();
    });
}

void MainFrame::buildMenuBar() {
    Bind(wxEVT_MENU, &MainFrame::onSettings, this, IdSettings);
    Bind(wxEVT_MENU, &MainFrame::onQuit,     this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::onAbout,    this, IdAbout);
    Bind(wxEVT_MENU, &MainFrame::onSwitchGame,        this, IdGameMenuBase, IdGameMenuLast);
    Bind(wxEVT_MENU, &MainFrame::onUpdateSetsForGame, this, IdSetsMenuBase, IdSetsMenuLast);
}

void MainFrame::buildLayout() {
    auto* root = new wxBoxSizer(wxVERTICAL);

    menuStrip_ = new wxPanel(this, wxID_ANY);
    auto* menuSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* fileLbl = new wxStaticText(menuStrip_, wxID_ANY, "File");
    auto* gameLbl = new wxStaticText(menuStrip_, wxID_ANY, "Game");
    auto* setsLbl = new wxStaticText(menuStrip_, wxID_ANY, "Sets");
    auto* helpLbl = new wxStaticText(menuStrip_, wxID_ANY, "Help");
    fileLbl->SetCursor(wxCursor(wxCURSOR_HAND));
    gameLbl->SetCursor(wxCursor(wxCURSOR_HAND));
    setsLbl->SetCursor(wxCursor(wxCURSOR_HAND));
    helpLbl->SetCursor(wxCursor(wxCURSOR_HAND));
    fileLbl->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { onOpenFileMenu(); });
    gameLbl->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { onOpenGameMenu(); });
    setsLbl->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { onOpenSetsMenu(); });
    helpLbl->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { onOpenHelpMenu(); });
    menuSizer->Add(fileLbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP | wxBOTTOM | wxRIGHT, 4);
    menuSizer->Add(gameLbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP | wxBOTTOM | wxRIGHT, 8);
    menuSizer->Add(setsLbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP | wxBOTTOM | wxRIGHT, 8);
    menuSizer->Add(helpLbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP | wxBOTTOM | wxRIGHT, 8);
    menuStrip_->SetSizer(menuSizer);
    root->Add(menuStrip_, 0, wxEXPAND);

    toolbarPanel_ = new wxPanel(this, wxID_ANY);
    auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
    auto makeToolBtn = [&](int id, const char* svg, const wxString& tip) {
        wxBitmap bmp = svgIconBitmap(svg, kToolbarIconPx, "#000000");
        auto* b = new wxBitmapButton(toolbarPanel_, id, bmp, wxDefaultPosition,
                                     wxDefaultSize,
                                     wxBU_EXACTFIT);
        b->SetToolTip(tip);
        return b;
    };
    toolbarButtons_[0] = makeToolBtn(IdCreate, kSvgToolbarAdd,    "Add Card");
    toolbarButtons_[1] = makeToolBtn(IdEdit,   kSvgToolbarEdit,   "Edit");
    toolbarButtons_[2] = makeToolBtn(IdDelete, kSvgToolbarDelete, "Delete");
    toolbar->AddSpacer(4);
    toolbar->Add(toolbarButtons_[0], 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    toolbar->Add(toolbarButtons_[1], 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    toolbar->Add(toolbarButtons_[2], 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    toolbar->AddStretchSpacer(1);
    filterInput_ = new wxTextCtrl(toolbarPanel_, wxID_ANY, "", wxDefaultPosition,
                                  wxSize(260, -1));
    filterInput_->SetHint(kFilterInputHint);
    toolbar->Add(filterInput_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxTOP | wxBOTTOM, 4);
    toolbarPanel_->SetSizer(toolbar);
    root->Add(toolbarPanel_, 0, wxEXPAND);

    contentHost_ = new wxPanel(this, wxID_ANY);
    auto* hostSizer = new wxBoxSizer(wxVERTICAL);
    splitter_ = new wxSplitterWindow(contentHost_, wxID_ANY, wxDefaultPosition,
                                     wxDefaultSize, wxSP_LIVE_UPDATE);
    splitter_->SetMinimumPaneSize(280);
    hostSizer->Add(splitter_, 1, wxEXPAND);
    contentHost_->SetSizer(hostSizer);
    root->Add(contentHost_, 1, wxEXPAND);

    auto* statusPanel = new wxPanel(this, wxID_ANY);
    auto* statusSizer = new wxBoxSizer(wxHORIZONTAL);
    statusText_ = new wxStaticText(statusPanel, wxID_ANY, "Ready");
    statusSizer->Add(statusText_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 6);
    statusPanel->SetSizer(statusSizer);
    root->Add(statusPanel, 0, wxEXPAND | wxTOP, 2);

    SetSizer(root);

    Bind(wxEVT_BUTTON, &MainFrame::onCreate, this, IdCreate);
    Bind(wxEVT_BUTTON, &MainFrame::onEdit,   this, IdEdit);
    Bind(wxEVT_BUTTON, &MainFrame::onDelete, this, IdDelete);

    filterInput_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        if (auto* view = activeView()) {
            view->setFilter(filterInput_->GetValue().ToStdString());
        }
    });
    filterInput_->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& ev) {
        const int code = ev.GetKeyCode();
        if (code == WXK_UP || code == WXK_DOWN) {
            if (auto* view = activeView()) {
                view->nudgeSelection(code == WXK_UP ? -1 : 1);
            }
            return;
        }
        ev.Skip();
    });

    // Selection changes are handled per-view (each IGameView binds
    // EVT_CARD_SELECTED on its own typed list panel and pushes the typed
    // selection into its selected panel). MainFrame reacts to status-bar
    // updates from preview fetches and list actions (e.g. clipboard copy).
    auto onStatusMessage = [this](wxCommandEvent& ev) {
        const wxString msg = ev.GetString();
        setStatusTextUi(msg.IsEmpty() ? wxString("Ready") : msg);
    };
    Bind(EVT_PREVIEW_STATUS, onStatusMessage);
    Bind(EVT_UI_STATUS, onStatusMessage);
}

IGameView* MainFrame::activeView() {
    for (auto* v : ctx_.gameViews) {
        if (v != nullptr && v->gameId() == activeGame_) return v;
    }
    return nullptr;
}

void MainFrame::mountActiveView() {
    auto* view = activeView();
    if (view == nullptr || splitter_ == nullptr || contentHost_ == nullptr) return;

    auto* hostSizer = contentHost_->GetSizer();
    if (hostSizer == nullptr) return;

    // Hide every other view's panels so wx doesn't double-paint them.
    for (auto* other : ctx_.gameViews) {
        if (other == nullptr || other == view) continue;
        if (other->hostsOwnLayout()) {
            if (auto* cp = other->contentPanelIfCreated()) cp->Hide();
            continue;
        }
        if (auto* lp = other->listPanel(splitter_)) lp->Hide();
        if (auto* sp = other->selectedPanel(splitter_)) sp->Hide();
    }

    const ThemePalette palette = paletteForTheme(ctx_.config.current().theme);

    if (toolbarPanel_ != nullptr) {
        if (view->hostsOwnLayout()) toolbarPanel_->Hide();
        else                        toolbarPanel_->Show();
        Layout();
    }

    if (view->hostsOwnLayout()) {
        auto* custom = view->contentPanel(contentHost_);
        if (custom == nullptr) return;

        splitter_->Hide();
        hostSizer->Clear(false);
        custom->Show();
        hostSizer->Add(custom, 1, wxEXPAND);
        contentHost_->Layout();

        // Digimon (and other hostsOwnLayout views) apply their own tree theme
        // and then restore tab-strip colors; a follow-up applyThemeToWindowTree
        // here would reset tab labels to panelBg and leave a dark box around text.
        view->applyTheme(palette);
        return;
    }

    if (auto* previousCustom = view->contentPanelIfCreated()) {
        previousCustom->Hide();
    }
    // Re-seat the shared splitter if a contentPanel game was showing.
    hostSizer->Clear(false);
    splitter_->Show();
    hostSizer->Add(splitter_, 1, wxEXPAND);

    auto* listPanel = view->listPanel(splitter_);
    auto* selectedPanel = view->selectedPanel(splitter_);
    if (listPanel == nullptr || selectedPanel == nullptr) return;
    listPanel->Show();
    selectedPanel->Show();

    if (splitter_->IsSplit()) {
        splitter_->ReplaceWindow(splitter_->GetWindow1(), selectedPanel);
        splitter_->ReplaceWindow(splitter_->GetWindow2(), listPanel);
    } else {
        splitter_->SplitVertically(selectedPanel, listPanel, 360);
    }

    contentHost_->Layout();
    view->applyTheme(palette);
    applyThemeToWindowTree(selectedPanel, palette, ctx_.config.current().theme);
    applyThemeToWindowTree(listPanel, palette, ctx_.config.current().theme);
}

void MainFrame::switchGame(Game g) {
    if (g == activeGame_) return;
    activeGame_ = g;
    mountActiveView();
    if (auto* view = activeView()) {
        if (filterInput_ != nullptr) {
            filterInput_->ChangeValue(wxString{});
            filterInput_->SetHint(kFilterInputHint);
            filterInput_->Refresh();
        }
        view->setFilter("");
        view->refreshCollection();
        setStatusTextUi(view->displayName());
    }
}

void MainFrame::refreshToolbarIcons() {
    const ThemePalette palette = paletteForTheme(ctx_.config.current().theme);
    const std::string tbHex = palette.buttonText.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
    if (toolbarButtons_[0]) toolbarButtons_[0]->SetBitmap(svgIconBitmap(kSvgToolbarAdd, kToolbarIconPx, tbHex.c_str()));
    if (toolbarButtons_[1]) toolbarButtons_[1]->SetBitmap(svgIconBitmap(kSvgToolbarEdit, kToolbarIconPx, tbHex.c_str()));
    if (toolbarButtons_[2]) toolbarButtons_[2]->SetBitmap(svgIconBitmap(kSvgToolbarDelete, kToolbarIconPx, tbHex.c_str()));
}

void MainFrame::applyTheme() {
    const Theme currentTheme = ctx_.config.current().theme;
    const ThemePalette palette = paletteForTheme(currentTheme);
    applyThemeToWindowTree(this, palette, currentTheme);
    SetBackgroundColour(palette.windowBg);
    SetForegroundColour(palette.text);
    if (filterInput_ != nullptr) {
        applyPaletteToTextCtrl(filterInput_, palette, currentTheme);
    }
    for (auto* view : ctx_.gameViews) {
        if (view != nullptr) view->applyTheme(palette);
    }
    refreshToolbarIcons();
    Refresh();
    Update();
}

void MainFrame::setStatusTextUi(const wxString& text) {
    if (statusText_ != nullptr) {
        statusText_->SetLabelText(text);
    }
}

void MainFrame::onOpenFileMenu() {
    wxMenu menu;
    menu.Append(IdSettings, "Settings...\tCtrl+,", "Open application settings");
    menu.AppendSeparator();
    menu.Append(wxID_EXIT, "Quit\tCtrl+Q", "Exit the application");
    if (menuStrip_ != nullptr) {
        menuStrip_->PopupMenu(&menu, 4, menuStrip_->GetSize().GetHeight());
    }
}

void MainFrame::onOpenGameMenu() {
    wxMenu menu;
    menuIdToGame_.clear();
    int id = IdGameMenuBase;
    for (auto* view : ctx_.gameViews) {
        if (view == nullptr) continue;
        menu.AppendRadioItem(id, view->displayName());
        menu.Check(id, view->gameId() == activeGame_);
        menuIdToGame_[id] = view->gameId();
        ++id;
    }
    if (menuStrip_ != nullptr) {
        menuStrip_->PopupMenu(&menu, 44, menuStrip_->GetSize().GetHeight());
    }
}

void MainFrame::onOpenSetsMenu() {
    wxMenu menu;
    menuIdToGame_.clear();
    int id = IdSetsMenuBase;
    for (auto* view : ctx_.gameViews) {
        if (view == nullptr) continue;
        menu.Append(id, view->updateSetsMenuLabel(),
                    "Refresh set list from the game's API");
        menuIdToGame_[id] = view->gameId();
        ++id;
    }
    if (menuStrip_ != nullptr) {
        menuStrip_->PopupMenu(&menu, 92, menuStrip_->GetSize().GetHeight());
    }
}

void MainFrame::onOpenHelpMenu() {
    wxMenu menu;
    menu.Append(IdAbout, "About", "About Card Collection Manager 3");
    if (menuStrip_ != nullptr) {
        menuStrip_->PopupMenu(&menu, 136, menuStrip_->GetSize().GetHeight());
    }
}

// Menu handlers ---------------------------------------------------------------

void MainFrame::onSettings(wxCommandEvent&) {
    const Theme beforeTheme = ctx_.config.current().theme;
    const std::string beforeDataStorage = ctx_.config.current().dataStorage;
    SettingsDialog dlg(this, ctx_.config);
    {
        const Theme currentTheme = ctx_.config.current().theme;
        const ThemePalette palette = paletteForTheme(currentTheme);
        applyThemeToWindowTree(&dlg, palette, currentTheme);
        dlg.SetBackgroundColour(palette.panelBg);
        dlg.SetForegroundColour(palette.text);
    }
    if (dlg.ShowModal() == wxID_OK) {
        const bool dataDirChanged = ctx_.config.current().dataStorage != beforeDataStorage;
        if (dataDirChanged) {
            ensureDataStorageScaffold(ctx_.config.current());
            for (auto* view : ctx_.gameViews) {
                if (view != nullptr) view->refreshCollection();
            }
        }
        if (ctx_.config.current().theme != beforeTheme) {
            applyTheme();
        }
    }
}

void MainFrame::onQuit(wxCommandEvent&) { Close(true); }

void MainFrame::onSwitchGame(wxCommandEvent& ev) {
    const auto it = menuIdToGame_.find(ev.GetId());
    if (it == menuIdToGame_.end()) return;
    switchGame(it->second);
}

void MainFrame::onUpdateSetsForGame(wxCommandEvent& ev) {
    const auto it = menuIdToGame_.find(ev.GetId());
    if (it == menuIdToGame_.end()) return;
    IGameView* targetView = nullptr;
    for (auto* v : ctx_.gameViews) {
        if (v != nullptr && v->gameId() == it->second) { targetView = v; break; }
    }
    if (targetView == nullptr) return;

    setStatusTextUi("Updating " + targetView->displayName() + " sets...");
    Update();
    const auto status = targetView->onUpdateSets(this);
    setStatusTextUi(status);
}

void MainFrame::onAbout(wxCommandEvent&) {
    wxDialog dlg(this, wxID_ANY, "About Card Collection Manager 3",
                 wxDefaultPosition, wxDefaultSize,
                 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* name = new wxStaticText(&dlg, wxID_ANY, "Card Collection Manager 3");
    auto* version = new wxStaticText(&dlg, wxID_ANY, wxString("Version: ") + kAppVersion);
    auto* desc = new wxStaticText(&dlg, wxID_ANY,
                                  "Desktop card collection manager for Magic and Pokemon.");
    wxFont titleFont = name->GetFont();
    titleFont.MakeBold().MakeLarger();
    name->SetFont(titleFont);

    root->Add(name, 0, wxALL, 10);
    root->Add(version, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    root->Add(desc, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    if (auto* buttons = dlg.CreateButtonSizer(wxOK)) {
        root->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
    }

    dlg.SetSizerAndFit(root);
    const wxSize fitSize = dlg.GetSize();
    dlg.SetSize(fitSize.GetWidth(), static_cast<int>(fitSize.GetHeight() * 1.10));
    const Theme currentTheme = ctx_.config.current().theme;
    const ThemePalette palette = paletteForTheme(currentTheme);
    applyThemeToWindowTree(&dlg, palette, currentTheme);
    dlg.SetBackgroundColour(palette.panelBg);
    dlg.SetForegroundColour(palette.text);
    dlg.CentreOnParent();
    dlg.ShowModal();
}

// Toolbar handlers ------------------------------------------------------------

void MainFrame::onCreate(wxCommandEvent&) {
    if (auto* view = activeView()) view->onAddCard(this);
}

void MainFrame::onEdit(wxCommandEvent&) {
    if (auto* view = activeView()) view->onEditCard(this);
}

void MainFrame::onDelete(wxCommandEvent&) {
    if (auto* view = activeView()) view->onDeleteCard(this);
}

}  // namespace ccm::ui
