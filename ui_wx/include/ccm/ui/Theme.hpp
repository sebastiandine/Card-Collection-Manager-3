#pragma once

#include "ccm/domain/Enums.hpp"

#include <wx/colour.h>

#include <cstddef>
#include <string>
#include <string_view>

class wxBitmapButton;
class wxDialog;
class wxWindow;
class wxString;
class wxTextCtrl;

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
// Force palette colors onto a text input (incl. MSW dark-mode typed-text fix).
void applyPaletteToTextCtrl(wxTextCtrl* text, const ThemePalette& palette, Theme theme);
void themeModalDialog(wxDialog* dlg, Theme theme);
int showThemedMessageDialog(wxWindow* parent, const wxString& message, const wxString& caption, long style);
int showThemedConfirmDialog(wxWindow* parent, const wxString& message, const wxString& caption);

// Show/hide the shared or per-game Edit toolbar button and reflow its sizer
// so Add/Delete close the gap when Edit is hidden for multi-select.
void setToolbarEditVisible(wxBitmapButton* edit, bool visible);

// Confirm copy for Delete: one card by name, or "Delete N selected entries?".
wxString deleteCardsConfirmMessage(std::size_t count, std::string_view singleCardName);

}  // namespace ccm::ui
