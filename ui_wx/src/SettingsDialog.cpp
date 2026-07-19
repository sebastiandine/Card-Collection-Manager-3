#include "ccm/ui/SettingsDialog.hpp"
#include "ccm/ui/Theme.hpp"
#include "ccm/domain/Enums.hpp"

#include <wx/button.h>
#include <wx/dirdlg.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace ccm::ui {

namespace {

wxString displayLabelForGame(Game g) {
    switch (g) {
        case Game::Magic:        return "Magic";
        case Game::Pokemon:      return "Pokemon";
        case Game::YuGiOh:       return "Yu-Gi-Oh!";
        case Game::DigiBattle99: return "Digimon (Digi-Battle)";
    }
    return wxString::FromUTF8(to_string(g).data());
}

}  // namespace

SettingsDialog::SettingsDialog(wxWindow* parent, ConfigService& config)
    : wxDialog(parent, wxID_ANY, "Settings",
               wxDefaultPosition, wxSize(560, 200),
               wxDEFAULT_DIALOG_STYLE),
      config_(config) {
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* dirRow = new wxBoxSizer(wxHORIZONTAL);
    dirRow->Add(new wxStaticText(this, wxID_ANY, "Data directory:"),
                0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    dataDirCtrl_ = new wxTextCtrl(this, wxID_ANY, config_.current().dataStorage);
    dirRow->Add(dataDirCtrl_, 1, wxEXPAND | wxRIGHT, 6);
    auto* browse = new wxButton(this, wxID_ANY, "Browse...");
    browse->Bind(wxEVT_BUTTON, &SettingsDialog::onBrowse, this);
    dirRow->Add(browse, 0);
    root->Add(dirRow, 0, wxEXPAND | wxALL, 10);

    auto* gameRow = new wxBoxSizer(wxHORIZONTAL);
    gameRow->Add(new wxStaticText(this, wxID_ANY, "Default game:"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    defaultGameChoice_ = new wxChoice(this, wxID_ANY);
    int selected = 0;
    int idx = 0;
    for (const Game g : allGames()) {
        defaultGameChoice_->Append(displayLabelForGame(g));
        if (g == config_.current().defaultGame) selected = idx;
        ++idx;
    }
    defaultGameChoice_->SetSelection(selected);
    gameRow->Add(defaultGameChoice_, 0);
    root->Add(gameRow, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

    auto* themeRow = new wxBoxSizer(wxHORIZONTAL);
    themeRow->Add(new wxStaticText(this, wxID_ANY, "Theme:"),
                  0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    themeChoice_ = new wxChoice(this, wxID_ANY);
    themeChoice_->Append("Light");
    themeChoice_->Append("Dark");
    switch (config_.current().theme) {
        case Theme::Dark:         themeChoice_->SetSelection(1); break;
        case Theme::Light:
        default:                  themeChoice_->SetSelection(0); break;
    }
    themeRow->Add(themeChoice_, 0);
    root->Add(themeRow, 0, wxEXPAND | wxALL, 10);

    auto* btns = CreateButtonSizer(wxOK | wxCANCEL);
    if (btns) root->Add(btns, 0, wxALL | wxEXPAND, 10);
    Bind(wxEVT_BUTTON, &SettingsDialog::onOk, this, wxID_OK);

    SetSizer(root);

    // Ensure long paths are initially shown from the start, not scrolled right.
    CallAfter([this]() {
        if (dataDirCtrl_) {
            dataDirCtrl_->SetInsertionPoint(0);
            dataDirCtrl_->ShowPosition(0);
        }
    });
}

void SettingsDialog::onBrowse(wxCommandEvent&) {
    wxDirDialog dlg(this, "Choose data directory",
                    dataDirCtrl_->GetValue(),
                    wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        dataDirCtrl_->SetValue(dlg.GetPath());
    }
}

void SettingsDialog::onOk(wxCommandEvent& ev) {
    Configuration next = config_.current();
    next.dataStorage = dataDirCtrl_->GetValue().ToStdString();
    const int gameSel = defaultGameChoice_->GetSelection();
    const auto& games = allGames();
    if (gameSel >= 0 && static_cast<std::size_t>(gameSel) < games.size()) {
        next.defaultGame = games[static_cast<std::size_t>(gameSel)];
    }
    switch (themeChoice_->GetSelection()) {
        case 1: next.theme = Theme::Dark; break;
        case 0:
        default:
            next.theme = Theme::Light;
            break;
    }
    auto stored = config_.store(std::move(next));
    if (!stored) {
        showThemedMessageDialog(this, "Failed to save settings: " + stored.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    ev.Skip();
}

}  // namespace ccm::ui
