#pragma once

// SettingsDialog: edits the live Configuration via ConfigService.

#include "ccm/services/ConfigService.hpp"

#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/textctrl.h>

namespace ccm::ui {

class SettingsDialog : public wxDialog {
public:
    SettingsDialog(wxWindow* parent, ConfigService& config);

private:
    void onBrowse(wxCommandEvent&);
    void onOk(wxCommandEvent&);

    ConfigService& config_;

    wxTextCtrl* dataDirCtrl_{nullptr};
    wxChoice*   defaultGameChoice_{nullptr};
    wxChoice*   themeChoice_{nullptr};
};

}  // namespace ccm::ui
