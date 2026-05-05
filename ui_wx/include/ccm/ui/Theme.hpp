#pragma once

#include "ccm/domain/Enums.hpp"

#include <wx/colour.h>

class wxWindow;

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

}  // namespace ccm::ui
