#pragma once

#include "ccm/domain/Enums.hpp"

#include <wx/colour.h>

class wxDialog;
class wxWindow;
class wxString;

namespace ccm::ui {

struct ThemePalette {
    wxColour windowBg;
    wxColour panelBg;
    wxColour text;
    wxColour inputBg;
    wxColour inputText;
    wxColour buttonBg;
    wxColour buttonText;
};

ThemePalette paletteForTheme(Theme theme);
Theme inferThemeFromWindow(const wxWindow* window);
void applyThemeToWindowTree(wxWindow* root, const ThemePalette& palette, Theme theme);
void themeModalDialog(wxDialog* dlg, Theme theme);
int showThemedMessageDialog(wxWindow* parent, const wxString& message, const wxString& caption, long style);
int showThemedConfirmDialog(wxWindow* parent, const wxString& message, const wxString& caption);

}  // namespace ccm::ui
