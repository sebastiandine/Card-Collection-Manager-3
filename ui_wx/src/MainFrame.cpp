#include "ccm/ui/MainFrame.hpp"

#include "ccm/ui/CardEditDialog.hpp"
#include "ccm/ui/MagicCardListPanel.hpp"
#include "ccm/ui/SelectedCardPanel.hpp"
#include "ccm/ui/SettingsDialog.hpp"
#include "ccm/ui/SvgIcons.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/bmpbuttn.h>
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

#include <optional>
#include <string>

#ifdef __WXMSW__
#include <windows.h>
#endif

namespace ccm::ui {

namespace {
constexpr int kToolbarIconPx = 18;
}  // namespace

MainFrame::MainFrame(AppContext& ctx)
    : wxFrame(nullptr, wxID_ANY, "Card Collection Manager 3",
              wxDefaultPosition, wxSize(1100, 700)),
      ctx_(ctx),
      activeGame_(ctx.config.current().defaultGame) {
    buildMenuBar();
    buildLayout();
    applyTheme();
    setStatusTextUi("Ready");

    setStatusTextUi("Loading collection...");
    // Defer initial load so the frame can paint first; this improves perceived
    // startup responsiveness on larger collections.
    CallAfter([this]() { refreshCollection(); });
}

void MainFrame::buildMenuBar() {
    Bind(wxEVT_MENU, &MainFrame::onSettings,          this, IdSettings);
    Bind(wxEVT_MENU, &MainFrame::onQuit,              this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::onSwitchMagic,       this, IdSwitchMagic);
    Bind(wxEVT_MENU, &MainFrame::onSwitchPokemon,     this, IdSwitchPokemon);
    Bind(wxEVT_MENU, &MainFrame::onUpdateSetsMagic,   this, IdUpdateSetsMagic);
    Bind(wxEVT_MENU, &MainFrame::onUpdateSetsPokemon, this, IdUpdateSetsPokemon);
}

void MainFrame::buildLayout() {
    auto* root = new wxBoxSizer(wxVERTICAL);

    menuStrip_ = new wxPanel(this, wxID_ANY);
    auto* menuSizer = new wxBoxSizer(wxHORIZONTAL);
    auto* fileLbl = new wxStaticText(menuStrip_, wxID_ANY, "File");
    auto* gameLbl = new wxStaticText(menuStrip_, wxID_ANY, "Game");
    auto* setsLbl = new wxStaticText(menuStrip_, wxID_ANY, "Sets");
    fileLbl->SetCursor(wxCursor(wxCURSOR_HAND));
    gameLbl->SetCursor(wxCursor(wxCURSOR_HAND));
    setsLbl->SetCursor(wxCursor(wxCURSOR_HAND));
    fileLbl->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { onOpenFileMenu(); });
    gameLbl->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { onOpenGameMenu(); });
    setsLbl->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { onOpenSetsMenu(); });
    menuSizer->Add(fileLbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP | wxBOTTOM | wxRIGHT, 4);
    menuSizer->Add(gameLbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP | wxBOTTOM | wxRIGHT, 8);
    menuSizer->Add(setsLbl, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP | wxBOTTOM | wxRIGHT, 8);
    menuStrip_->SetSizer(menuSizer);
    root->Add(menuStrip_, 0, wxEXPAND);

    // Legacy top-row layout: icon action buttons on the left (the original TS uses
    // react-icons/vsc VscAdd/VscEdit/VscTrash — we embed the matching
    // vscode-codicons SVGs via SvgIcons), filter field on the right.
    auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
    auto makeToolBtn = [&](int id, const char* svg, const wxString& tip) {
        wxBitmap bmp = svgIconBitmap(svg, kToolbarIconPx, "#000000");
        auto* b = new wxBitmapButton(this, id, bmp, wxDefaultPosition,
                                     wxDefaultSize,
                                     wxBU_EXACTFIT);
        b->SetToolTip(tip);
        return b;
    };
    toolbarButtons_[0] = makeToolBtn(IdCreate, kSvgToolbarAdd,    "Add Card");
    toolbarButtons_[1] = makeToolBtn(IdEdit,   kSvgToolbarEdit,   "Edit");
    toolbarButtons_[2] = makeToolBtn(IdDelete, kSvgToolbarDelete, "Delete");
    // Keep the first toolbar button aligned with the left edge of "File".
    toolbar->AddSpacer(4);
    toolbar->Add(toolbarButtons_[0],
                  0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    toolbar->Add(toolbarButtons_[1],
                  0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    toolbar->Add(toolbarButtons_[2],
                  0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    toolbar->AddStretchSpacer(1);
    filterInput_ = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition,
                                  wxSize(260, -1));
    filterInput_->SetHint("Filter");
    toolbar->Add(filterInput_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxTOP | wxBOTTOM, 4);
    root->Add(toolbar, 0, wxEXPAND);

    auto* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition,
                                          wxDefaultSize, wxSP_LIVE_UPDATE);
    selectedPanel_ = new SelectedCardPanel(splitter, ctx_.images, ctx_.cardPreview);
    magicList_     = new MagicCardListPanel(splitter);
    splitter->SplitVertically(selectedPanel_, magicList_, 360);
    splitter->SetMinimumPaneSize(280);
    root->Add(splitter, 1, wxEXPAND);

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

    // Filter input -> table rows. Pushing every keystroke straight into
    // setFilter() is fine because rebuildRows itself is cheap (no I/O) and
    // the rebuild guard inside MagicCardListPanel collapses the resulting
    // wxListCtrl event burst into a single bubbled selection event.
    filterInput_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        magicList_->setFilter(filterInput_->GetValue().ToStdString());
    });

    // List selection -> details panel.
    Bind(EVT_MAGIC_CARD_SELECTED, [this](wxCommandEvent&) {
        selectedPanel_->setCard(magicList_->selected());
    });

    // Preview fetch outcome -> bottom status bar. The side panel emits this
    // every time a Scryfall preview resolves: empty string clears the bar
    // (success path), non-empty surfaces the underlying error so the user
    // knows it's not their connection / they're not staring at a stale
    // image. This is also where transient Scryfall failures (HTTP 429 rate
    // limits, network blips) become visible.
    Bind(EVT_PREVIEW_STATUS, [this](wxCommandEvent& ev) {
        const wxString msg = ev.GetString();
        setStatusTextUi(msg.IsEmpty() ? wxString("Ready") : msg);
    });
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
        // On Windows, native edit controls may keep default text color even
        // after tree theming; force the filter field to use palette input
        // colors so dark mode text stays readable while typing.
        filterInput_->SetBackgroundColour(palette.inputBg);
        filterInput_->SetForegroundColour(palette.inputText);
        filterInput_->SetOwnBackgroundColour(palette.inputBg);
        filterInput_->SetOwnForegroundColour(palette.inputText);
        filterInput_->Refresh();
    }
    if (magicList_) {
        magicList_->applyTheme(palette);
    }
    if (selectedPanel_) {
        selectedPanel_->applyTheme(palette);
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
    menu.AppendRadioItem(IdSwitchMagic, "Magic");
    menu.AppendRadioItem(IdSwitchPokemon, "Pokemon (coming soon)");
    menu.Check(IdSwitchMagic, activeGame_ == Game::Magic);
    menu.Check(IdSwitchPokemon, activeGame_ == Game::Pokemon);
    menu.Enable(IdSwitchPokemon, false);
    if (menuStrip_ != nullptr) {
        menuStrip_->PopupMenu(&menu, 44, menuStrip_->GetSize().GetHeight());
    }
}

void MainFrame::onOpenSetsMenu() {
    wxMenu menu;
    menu.Append(IdUpdateSetsMagic, "Update Magic", "Refresh set list from Scryfall");
    menu.Append(IdUpdateSetsPokemon, "Update Pokemon (coming soon)");
    menu.Enable(IdUpdateSetsPokemon, false);
    if (menuStrip_ != nullptr) {
        menuStrip_->PopupMenu(&menu, 92, menuStrip_->GetSize().GetHeight());
    }
}

void MainFrame::switchGame(Game g) {
    activeGame_ = g;
    refreshCollection();
    setStatusTextUi(g == Game::Magic ? "Magic" : "Pokemon");
}

void MainFrame::refreshCollection() {
    if (activeGame_ != Game::Magic) {
        magicList_->setCards({});
        selectedPanel_->setCard(std::nullopt);
        return;
    }
    auto loaded = ctx_.magicCollection.list(Game::Magic);
    if (!loaded) {
        wxMessageBox("Failed to load collection: " + loaded.error(),
                     "Error", wxOK | wxICON_ERROR, this);
        return;
    }
    magicList_->setCards(std::move(loaded).value());
    selectedPanel_->setCard(magicList_->selected());
}

const std::vector<Set>& MainFrame::magicSetsForDialog() {
    if (!magicSetsCache_.empty()) {
        return magicSetsCache_;
    }
    auto loaded = ctx_.sets.getSets(Game::Magic);
    if (loaded) {
        magicSetsCache_ = std::move(loaded).value();
    } else {
        magicSetsCache_.clear();
    }
    return magicSetsCache_;
}

// Menu handlers ---------------------------------------------------------------

void MainFrame::onSettings(wxCommandEvent&) {
    const Theme beforeTheme = ctx_.config.current().theme;
    SettingsDialog dlg(this, ctx_.config);
    {
        const Theme currentTheme = ctx_.config.current().theme;
        const ThemePalette palette = paletteForTheme(currentTheme);
        applyThemeToWindowTree(&dlg, palette, currentTheme);
        dlg.SetBackgroundColour(palette.panelBg);
        dlg.SetForegroundColour(palette.text);
    }
    if (dlg.ShowModal() == wxID_OK) {
        if (ctx_.config.current().theme != beforeTheme) {
            applyTheme();
        }
    }
}

void MainFrame::onQuit(wxCommandEvent&) { Close(true); }

void MainFrame::onSwitchMagic(wxCommandEvent&)   { switchGame(Game::Magic); }
void MainFrame::onSwitchPokemon(wxCommandEvent&) { switchGame(Game::Pokemon); }

void MainFrame::onUpdateSetsMagic(wxCommandEvent&) {
    setStatusTextUi("Updating Magic sets from Scryfall...");
    Update();
    auto out = ctx_.sets.updateSets(Game::Magic);
    if (!out) {
        setStatusTextUi("Update failed");
        wxMessageBox("Failed to update sets: " + out.error(),
                     "Error", wxOK | wxICON_ERROR, this);
        return;
    }
    magicSetsCache_ = out.value();
    setStatusTextUi("Magic sets updated.");
    wxMessageBox("Updated " + std::to_string(out.value().size()) + " Magic sets.",
                 "Sets updated", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::onUpdateSetsPokemon(wxCommandEvent&) {
    wxMessageBox("Pokemon support is not implemented yet.",
                 "Coming soon", wxOK | wxICON_INFORMATION, this);
}

// Toolbar handlers ------------------------------------------------------------

void MainFrame::onCreate(wxCommandEvent&) {
    if (activeGame_ != Game::Magic) {
        wxMessageBox("Pokemon support is not implemented yet.",
                     "Coming soon", wxOK | wxICON_INFORMATION, this);
        return;
    }
    MagicCard fresh;
    fresh.amount = 1;
    fresh.language = Language::English;
    fresh.condition = Condition::NearMint;

    CardEditDialog dlg(this, ctx_.images, ctx_.sets, EditMode::Create, fresh,
                       &magicSetsForDialog());
    {
        const Theme currentTheme = ctx_.config.current().theme;
        const ThemePalette palette = paletteForTheme(currentTheme);
        applyThemeToWindowTree(&dlg, palette, currentTheme);
        dlg.SetBackgroundColour(palette.panelBg);
        dlg.SetForegroundColour(palette.text);
    }
    if (dlg.ShowModal() != wxID_OK) return;

    auto added = ctx_.magicCollection.add(Game::Magic, dlg.card());
    if (!added) {
        wxMessageBox("Failed to add card: " + added.error(),
                     "Error", wxOK | wxICON_ERROR, this);
        return;
    }
    refreshCollection();
}

void MainFrame::onEdit(wxCommandEvent&) {
    auto sel = magicList_->selected();
    if (!sel) {
        wxMessageBox("Select a card first.", "Edit", wxOK | wxICON_INFORMATION, this);
        return;
    }
    CardEditDialog dlg(this, ctx_.images, ctx_.sets, EditMode::Edit, *sel,
                       &magicSetsForDialog());
    {
        const Theme currentTheme = ctx_.config.current().theme;
        const ThemePalette palette = paletteForTheme(currentTheme);
        applyThemeToWindowTree(&dlg, palette, currentTheme);
        dlg.SetBackgroundColour(palette.panelBg);
        dlg.SetForegroundColour(palette.text);
    }
    if (dlg.ShowModal() != wxID_OK) return;
    auto updated = ctx_.magicCollection.update(Game::Magic, dlg.card());
    if (!updated) {
        wxMessageBox("Failed to update card: " + updated.error(),
                     "Error", wxOK | wxICON_ERROR, this);
        return;
    }
    refreshCollection();
}

void MainFrame::onDelete(wxCommandEvent&) {
    auto sel = magicList_->selected();
    if (!sel) {
        wxMessageBox("Select a card first.", "Delete", wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (wxMessageBox("Delete \"" + sel->name + "\"?",
                     "Confirm", wxYES_NO | wxICON_QUESTION, this) != wxYES) {
        return;
    }
    auto removed = ctx_.magicCollection.remove(Game::Magic, sel->id);
    if (!removed) {
        wxMessageBox("Failed to delete card: " + removed.error(),
                     "Error", wxOK | wxICON_ERROR, this);
        return;
    }
    refreshCollection();
}

#ifdef __WXMSW__
WXLRESULT MainFrame::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) {
    if (message == WM_CTLCOLOREDIT && filterInput_ != nullptr) {
        const HWND target = reinterpret_cast<HWND>(lParam);
        const HWND filterHwnd = reinterpret_cast<HWND>(filterInput_->GetHandle());
        if (target != nullptr && filterHwnd != nullptr && target == filterHwnd) {
            const ThemePalette palette = paletteForTheme(ctx_.config.current().theme);
            HDC hdc = reinterpret_cast<HDC>(wParam);
            ::SetTextColor(hdc, RGB(palette.inputText.Red(), palette.inputText.Green(),
                                    palette.inputText.Blue()));
            ::SetBkColor(hdc, RGB(palette.inputBg.Red(), palette.inputBg.Green(),
                                  palette.inputBg.Blue()));
            ::SetDCBrushColor(hdc, RGB(palette.inputBg.Red(), palette.inputBg.Green(),
                                       palette.inputBg.Blue()));
            return reinterpret_cast<WXLRESULT>(::GetStockObject(DC_BRUSH));
        }
    }
    return wxFrame::MSWWindowProc(message, wParam, lParam);
}
#endif

}  // namespace ccm::ui
