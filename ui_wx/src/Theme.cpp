#include "ccm/ui/Theme.hpp"

#include <wx/button.h>
#include <wx/bmpbuttn.h>
#include <wx/choice.h>
#include <wx/dcbuffer.h>
#include <wx/frame.h>
#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/statusbr.h>
#include <wx/spinctrl.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/settings.h>
#include <wx/textctrl.h>
#include <wx/toplevel.h>
#include <wx/window.h>

#include <unordered_set>

#ifdef __WXMSW__
#include <windows.h>
#include <commctrl.h>
#endif

namespace ccm::ui {

namespace {
std::unordered_set<wxWindow*> gButtonHoverBound;
std::unordered_set<wxWindow*> gDialogGripLayoutBound;
struct ButtonVisualState {
    wxColour normalBg;
    wxColour hoverBg;
    wxColour pressedBg;
    wxColour text;
    bool darkLike{false};
};
std::unordered_map<wxWindow*, ButtonVisualState> gButtonVisualStates;
struct GripVisualState {
    wxColour bg;
    wxColour line;
};
std::unordered_map<wxWindow*, GripVisualState> gGripVisualStates;

wxColour lightenTowardWhite(const wxColour& c, int amount) {
    auto lift = [amount](unsigned char channel) -> unsigned char {
        const int raised = static_cast<int>(channel) + amount;
        return static_cast<unsigned char>(raised > 255 ? 255 : raised);
    };
    return wxColour(lift(c.Red()), lift(c.Green()), lift(c.Blue()));
}

bool isDarkLikeTheme(Theme theme) {
    return theme == Theme::Dark;
}

void ensureDarkDialogResizeGrip(wxWindow* window, const ThemePalette& palette, Theme theme) {
    auto* dialog = dynamic_cast<wxDialog*>(window);
    if (dialog == nullptr) return;
    if ((dialog->GetWindowStyleFlag() & wxRESIZE_BORDER) == 0) return;

    constexpr int kGripSize = 16;
    const wxString kGripName = "ccm_dark_resize_grip_overlay";
    wxWindow* grip = wxWindow::FindWindowByName(kGripName, dialog);

    if (!isDarkLikeTheme(theme)) {
        if (grip != nullptr) {
            gGripVisualStates.erase(grip);
            grip->Destroy();
        }
        return;
    }

    if (grip == nullptr) {
        grip = new wxWindow(dialog, wxID_ANY, wxDefaultPosition, wxSize(kGripSize, kGripSize),
                            wxBORDER_NONE);
        grip->SetName(kGripName);
        grip->SetCursor(wxCursor(wxCURSOR_SIZENWSE));
        grip->SetBackgroundStyle(wxBG_STYLE_PAINT);

        grip->Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});
        grip->Bind(wxEVT_PAINT, [grip](wxPaintEvent&) {
            wxAutoBufferedPaintDC dc(grip);
            const auto it = gGripVisualStates.find(grip);
            const wxColour bg = (it != gGripVisualStates.end()) ? it->second.bg : wxColour(45, 45, 45);
            const wxColour line = (it != gGripVisualStates.end()) ? it->second.line : wxColour(110, 110, 110);

            const wxRect rect = grip->GetClientRect();
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(bg));
            dc.DrawRectangle(rect);

            dc.SetPen(wxPen(line, 1));
            const int r = rect.GetRight();
            const int b = rect.GetBottom();
            dc.DrawLine(r - 11, b, r, b - 11);
            dc.DrawLine(r - 7,  b, r, b - 7);
            dc.DrawLine(r - 3,  b, r, b - 3);
        });
#ifdef __WXMSW__
        grip->Bind(wxEVT_LEFT_DOWN, [dialog](wxMouseEvent&) {
            const HWND hwnd = reinterpret_cast<HWND>(dialog->GetHandle());
            if (hwnd == nullptr) return;
            ::ReleaseCapture();
            ::SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, 0);
        });
#endif
        grip->Bind(wxEVT_DESTROY, [grip](wxWindowDestroyEvent& ev) {
            gGripVisualStates.erase(grip);
            ev.Skip();
        });
    }

    gGripVisualStates[grip] = GripVisualState{
        palette.panelBg,
        lightenTowardWhite(palette.panelBg, 48),
    };

    auto placeGrip = [dialog, grip]() {
        const wxSize cs = dialog->GetClientSize();
        const int w = kGripSize;
        const int h = kGripSize;
        grip->SetSize(std::max(0, cs.GetWidth() - w), std::max(0, cs.GetHeight() - h), w, h);
        grip->Raise();
    };
    placeGrip();
    grip->Show();
    grip->Refresh();

    if (!gDialogGripLayoutBound.count(dialog)) {
        gDialogGripLayoutBound.insert(dialog);
        dialog->Bind(wxEVT_SIZE, [dialog](wxSizeEvent& ev) {
            if (wxWindow* w = wxWindow::FindWindowByName("ccm_dark_resize_grip_overlay", dialog)) {
                constexpr int kSize = 16;
                const wxSize cs = dialog->GetClientSize();
                w->SetSize(std::max(0, cs.GetWidth() - kSize), std::max(0, cs.GetHeight() - kSize), kSize, kSize);
                w->Raise();
            }
            ev.Skip();
        });
        dialog->Bind(wxEVT_DESTROY, [dialog](wxWindowDestroyEvent& ev) {
            gDialogGripLayoutBound.erase(dialog);
            ev.Skip();
        });
    }
}
}

#ifdef __WXMSW__
namespace {

using SetWindowThemeFn = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
using AllowDarkModeForWindowFn = BOOL(WINAPI*)(HWND, BOOL);
enum class PreferredAppMode : int {
    Default = 0,
    AllowDark = 1,
    ForceDark = 2,
    ForceLight = 3,
    Max = 4
};
using SetPreferredAppModeFn = PreferredAppMode(WINAPI*)(PreferredAppMode);
using FlushMenuThemesFn = VOID(WINAPI*)();

#ifndef HDM_SETBKCOLOR
#define HDM_SETBKCOLOR (HDM_FIRST + 19)
#endif
#ifndef HDM_SETTEXTCOLOR
#define HDM_SETTEXTCOLOR (HDM_FIRST + 20)
#endif

SetWindowThemeFn resolveSetWindowTheme() {
    static HMODULE uxthemeModule = ::LoadLibraryW(L"uxtheme.dll");
    static auto setWindowTheme = reinterpret_cast<SetWindowThemeFn>(
        uxthemeModule ? ::GetProcAddress(uxthemeModule, "SetWindowTheme") : nullptr);
    return setWindowTheme;
}

AllowDarkModeForWindowFn resolveAllowDarkModeForWindow() {
    static HMODULE uxthemeModule = ::LoadLibraryW(L"uxtheme.dll");
    static auto fn = reinterpret_cast<AllowDarkModeForWindowFn>(
        uxthemeModule ? ::GetProcAddress(uxthemeModule, MAKEINTRESOURCEA(133)) : nullptr);
    return fn;
}

SetPreferredAppModeFn resolveSetPreferredAppMode() {
    static HMODULE uxthemeModule = ::LoadLibraryW(L"uxtheme.dll");
    static auto fn = reinterpret_cast<SetPreferredAppModeFn>(
        uxthemeModule ? ::GetProcAddress(uxthemeModule, MAKEINTRESOURCEA(135)) : nullptr);
    return fn;
}

FlushMenuThemesFn resolveFlushMenuThemes() {
    static HMODULE uxthemeModule = ::LoadLibraryW(L"uxtheme.dll");
    static auto fn = reinterpret_cast<FlushMenuThemesFn>(
        uxthemeModule ? ::GetProcAddress(uxthemeModule, MAKEINTRESOURCEA(136)) : nullptr);
    return fn;
}

void applyNativeClassTheme(wxWindow* window, Theme theme, const wchar_t* darkClass, const wchar_t* lightClass) {
    if (window == nullptr) return;
    const HWND hwnd = reinterpret_cast<HWND>(window->GetHandle());
    if (hwnd == nullptr) return;

    const auto setWindowTheme = resolveSetWindowTheme();
    if (setWindowTheme == nullptr) return;

    const bool dark = (theme == Theme::Dark);
    setWindowTheme(hwnd, dark ? darkClass : lightClass, nullptr);
}

COLORREF toColorRef(const wxColour& c) {
    return RGB(c.Red(), c.Green(), c.Blue());
}

void applyListHeaderTheme(wxWindow* window, Theme theme, const ThemePalette& palette) {
    auto* list = dynamic_cast<wxListCtrl*>(window);
    if (list == nullptr) return;

    const HWND listHwnd = reinterpret_cast<HWND>(list->GetHandle());
    if (listHwnd == nullptr) return;

    const auto setWindowTheme = resolveSetWindowTheme();
    if (setWindowTheme == nullptr) return;

    const HWND header = ListView_GetHeader(listHwnd);
    if (header == nullptr) return;

    const bool dark = (theme == Theme::Dark);
    if (auto allowDarkModeForWindow = resolveAllowDarkModeForWindow()) {
        allowDarkModeForWindow(header, dark ? TRUE : FALSE);
    }
    if (dark) {
        // Different Windows builds react to different class tokens.
        setWindowTheme(header, L"DarkMode_ItemsView", nullptr);
        setWindowTheme(header, L"DarkMode_Explorer", nullptr);
        setWindowTheme(header, L"ItemsView", nullptr);
    } else {
        setWindowTheme(header, L"Header", nullptr);
        setWindowTheme(header, L"ItemsView", nullptr);
    }

    // Force the native header colors to match the selected app theme.
    ::SendMessageW(header, HDM_SETBKCOLOR, 0, static_cast<LPARAM>(toColorRef(palette.inputBg)));
    ::SendMessageW(header, HDM_SETTEXTCOLOR, 0, static_cast<LPARAM>(toColorRef(palette.inputText)));
    InvalidateRect(header, nullptr, TRUE);
}

void applyFrameTitlebarTheme(wxWindow* window, Theme theme) {
    if (dynamic_cast<wxTopLevelWindow*>(window) == nullptr) return;

    const HWND hwnd = reinterpret_cast<HWND>(window->GetHandle());
    if (hwnd == nullptr) return;

    static HMODULE dwmModule = ::LoadLibraryW(L"dwmapi.dll");
    static auto dwmSetWindowAttribute = reinterpret_cast<DwmSetWindowAttributeFn>(
        dwmModule ? ::GetProcAddress(dwmModule, "DwmSetWindowAttribute") : nullptr);
    if (dwmSetWindowAttribute == nullptr) return;

    const bool darkLike = (theme == Theme::Dark);
    const BOOL useDark = darkLike ? TRUE : FALSE;
    constexpr DWORD kDwmUseImmersiveDarkModeOld = 19;
    constexpr DWORD kDwmUseImmersiveDarkModeNew = 20;
    dwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkModeOld, &useDark, sizeof(useDark));
    dwmSetWindowAttribute(hwnd, kDwmUseImmersiveDarkModeNew, &useDark, sizeof(useDark));

    // Ask uxtheme to use dark menu rendering for the top menu strip.
    if (auto setPreferredAppMode = resolveSetPreferredAppMode()) {
        setPreferredAppMode(darkLike ? PreferredAppMode::ForceDark : PreferredAppMode::Default);
    }
    if (auto allowDarkModeForWindow = resolveAllowDarkModeForWindow()) {
        allowDarkModeForWindow(hwnd, useDark);
    }
    if (auto setWindowTheme = resolveSetWindowTheme()) {
        // Ensure top-level non-client rendering (including resize grip/corner)
        // uses a dark-capable class theme when the app is in dark mode.
        setWindowTheme(hwnd, darkLike ? L"DarkMode_Explorer" : L"Explorer", nullptr);
    }
    if (auto flushMenuThemes = resolveFlushMenuThemes()) {
        flushMenuThemes();
    }
    DrawMenuBar(hwnd);
}

void applyTopLevelSizeGripTheme(wxWindow* window, Theme theme) {
    if (dynamic_cast<wxTopLevelWindow*>(window) == nullptr) return;

    const HWND top = reinterpret_cast<HWND>(window->GetHandle());
    if (top == nullptr) return;

    const auto setWindowTheme = resolveSetWindowTheme();
    if (setWindowTheme == nullptr) return;

    const bool dark = (theme == Theme::Dark);
    const BOOL useDark = dark ? TRUE : FALSE;

    std::pair<Theme, SetWindowThemeFn> enumCtx{theme, setWindowTheme};

    ::EnumChildWindows(
        top,
        [](HWND child, LPARAM lParam) -> BOOL {
            auto* ctx = reinterpret_cast<std::pair<Theme, SetWindowThemeFn>*>(lParam);
            if (ctx == nullptr || ctx->second == nullptr) return TRUE;

            wchar_t className[64] = {};
            if (::GetClassNameW(child, className, static_cast<int>(sizeof(className) / sizeof(className[0]))) <= 0) {
                return TRUE;
            }
            const LONG_PTR style = ::GetWindowLongPtrW(child, GWL_STYLE);
            const bool isScrollbarClass = (::wcscmp(className, L"SCROLLBAR") == 0);
            const bool isStatusbarClass = (::wcscmp(className, STATUSCLASSNAMEW) == 0);
            const bool isSizeGrip =
                (style & SBS_SIZEGRIP) != 0 ||
                (style & SBS_SIZEBOX) != 0 ||
                (style & SBS_SIZEBOXBOTTOMRIGHTALIGN) != 0 ||
                (style & SBS_SIZEBOXTOPLEFTALIGN) != 0 ||
                (style & SBARS_SIZEGRIP) != 0;
            if (!isSizeGrip) return TRUE;
            if (!isScrollbarClass && !isStatusbarClass) return TRUE;


            const bool darkLocal = (ctx->first == Theme::Dark);
            if (auto allowDarkModeForWindow = resolveAllowDarkModeForWindow()) {
                allowDarkModeForWindow(child, darkLocal ? TRUE : FALSE);
            }
            const wchar_t* darkClass = isStatusbarClass ? L"DarkMode_StatusBar" : L"DarkMode_Explorer";
            const wchar_t* lightClass = isStatusbarClass ? L"Status" : L"Explorer";
            ctx->second(child, darkLocal ? darkClass : lightClass, nullptr);
            ::InvalidateRect(child, nullptr, TRUE);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&enumCtx));

    if (auto allowDarkModeForWindow = resolveAllowDarkModeForWindow()) {
        allowDarkModeForWindow(top, useDark);
    }
}

}  // namespace
#endif

ThemePalette paletteForTheme(Theme theme) {
    switch (theme) {
        case Theme::Dark:
            return ThemePalette{
                wxColour(30, 30, 30),
                wxColour(45, 45, 45),
                wxColour(230, 230, 230),
                wxColour(60, 60, 60),
                wxColour(230, 230, 230),
                wxColour(75, 75, 75),
                wxColour(240, 240, 240),
            };
        case Theme::Light:
        default:
            return ThemePalette{
                wxColour(248, 248, 248),
                wxColour(255, 255, 255),
                wxColour(20, 20, 20),
                wxColour(255, 255, 255),
                wxColour(20, 20, 20),
                wxColour(245, 245, 245),
                wxColour(20, 20, 20),
            };
    }
}

Theme inferThemeFromWindow(const wxWindow* window) {
    if (window == nullptr) return Theme::Light;

    const wxWindow* probe = window;
    wxColour bg;
    while (probe != nullptr) {
        bg = probe->GetBackgroundColour();
        if (bg.IsOk()) break;
        probe = probe->GetParent();
    }
    if (!bg.IsOk()) {
        bg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    }

    const int luminance =
        (299 * bg.Red() + 587 * bg.Green() + 114 * bg.Blue()) / 1000;
    return luminance < 128 ? Theme::Dark : Theme::Light;
}

void applyThemeToWindowTree(wxWindow* root, const ThemePalette& palette, Theme theme) {
    if (root == nullptr) return;

    root->SetForegroundColour(palette.text);
    root->SetBackgroundColour(palette.panelBg);
    root->SetOwnForegroundColour(palette.text);
    root->SetOwnBackgroundColour(palette.panelBg);

#ifdef __WXMSW__
    applyFrameTitlebarTheme(root, theme);
    applyTopLevelSizeGripTheme(root, theme);
#endif
    ensureDarkDialogResizeGrip(root, palette, theme);

    if (dynamic_cast<wxTextCtrl*>(root) != nullptr ||
        dynamic_cast<wxListCtrl*>(root) != nullptr ||
        dynamic_cast<wxListBox*>(root) != nullptr ||
        dynamic_cast<wxChoice*>(root) != nullptr ||
        dynamic_cast<wxSpinCtrl*>(root) != nullptr) {
        if (auto* text = dynamic_cast<wxTextCtrl*>(root)) {
            // On Windows, themed EDIT controls can ignore wx foreground color
            // while typing in dark mode; disable native theming there so the
            // control consistently uses palette-driven text/background colors.
            text->SetThemeEnabled(!isDarkLikeTheme(theme));
        }
        root->SetBackgroundColour(palette.inputBg);
        root->SetForegroundColour(palette.inputText);
        root->SetOwnBackgroundColour(palette.inputBg);
        root->SetOwnForegroundColour(palette.inputText);
#ifdef __WXMSW__
        if (dynamic_cast<wxListCtrl*>(root) != nullptr) {
            // Keep both native list scrollbars and the SysHeader32 control themed.
            applyNativeClassTheme(root, theme, L"DarkMode_Explorer", L"Explorer");
            applyListHeaderTheme(root, theme, palette);
        } else if (dynamic_cast<wxTextCtrl*>(root) != nullptr) {
            // Do not apply Explorer class theming to edit controls: on some
            // Windows builds it forces black typed text in dark mode.
            // Keep text fields palette-driven via wx colors instead.
        } else {
            applyNativeClassTheme(root, theme, L"DarkMode_Explorer", L"Explorer");
        }
#endif
    }

    if (dynamic_cast<wxStatusBar*>(root) != nullptr) {
        root->SetBackgroundColour(palette.panelBg);
        root->SetForegroundColour(palette.text);
        root->SetOwnBackgroundColour(palette.panelBg);
        root->SetOwnForegroundColour(palette.text);
#ifdef __WXMSW__
        applyNativeClassTheme(root, theme, L"DarkMode_StatusBar", L"Status");
#endif
    }

    if (dynamic_cast<wxButton*>(root) != nullptr ||
        dynamic_cast<wxBitmapButton*>(root) != nullptr) {
        const bool darkLike = isDarkLikeTheme(theme);
        root->SetThemeEnabled(!darkLike);
        root->SetBackgroundColour(palette.buttonBg);
        root->SetForegroundColour(palette.buttonText);
        root->SetOwnBackgroundColour(palette.buttonBg);
        root->SetOwnForegroundColour(palette.buttonText);

        const wxColour normalBg = palette.buttonBg;
        const int hoverLift = 18;
        const int pressedLift = 30;
        const wxColour hoverBg = darkLike ? lightenTowardWhite(normalBg, hoverLift) : normalBg;
        const wxColour pressedBg = darkLike ? lightenTowardWhite(normalBg, pressedLift) : normalBg;
        const wxColour btnFg = palette.buttonText;
        gButtonVisualStates[root] = ButtonVisualState{
            normalBg, hoverBg, pressedBg, btnFg, darkLike
        };

        if (!gButtonHoverBound.count(root)) {
            gButtonHoverBound.insert(root);

            root->Bind(wxEVT_ENTER_WINDOW, [root](wxMouseEvent& event) {
                const auto it = gButtonVisualStates.find(root);
                if (it == gButtonVisualStates.end() || !it->second.darkLike) {
                    event.Skip();
                    return;
                }
                root->SetBackgroundColour(it->second.hoverBg);
                root->SetForegroundColour(it->second.text);
                root->Refresh();
            });
            root->Bind(wxEVT_LEAVE_WINDOW, [root](wxMouseEvent& event) {
                const auto it = gButtonVisualStates.find(root);
                if (it == gButtonVisualStates.end() || !it->second.darkLike) {
                    event.Skip();
                    return;
                }
                root->SetBackgroundColour(it->second.normalBg);
                root->SetForegroundColour(it->second.text);
                root->Refresh();
            });
            root->Bind(wxEVT_LEFT_DOWN, [root](wxMouseEvent& event) {
                const auto it = gButtonVisualStates.find(root);
                if (it == gButtonVisualStates.end() || !it->second.darkLike) {
                    event.Skip();
                    return;
                }
                root->SetBackgroundColour(it->second.pressedBg);
                root->SetForegroundColour(it->second.text);
                root->Refresh();
                event.Skip();
            });
            root->Bind(wxEVT_LEFT_UP, [root](wxMouseEvent& event) {
                const auto it = gButtonVisualStates.find(root);
                if (it == gButtonVisualStates.end() || !it->second.darkLike) {
                    event.Skip();
                    return;
                }
                const wxPoint mousePos = wxGetMousePosition();
                const wxPoint localPos = root->ScreenToClient(mousePos);
                const bool inside = root->GetClientRect().Contains(localPos);
                root->SetBackgroundColour(inside ? it->second.hoverBg : it->second.normalBg);
                root->SetForegroundColour(it->second.text);
                root->Refresh();
                event.Skip();
            });
            root->Bind(wxEVT_SET_FOCUS, [root](wxFocusEvent& event) {
                const auto it = gButtonVisualStates.find(root);
                if (it == gButtonVisualStates.end() || !it->second.darkLike) {
                    event.Skip();
                    return;
                }
                root->SetBackgroundColour(it->second.hoverBg);
                root->SetForegroundColour(it->second.text);
                root->Refresh();
                event.Skip();
            });
            root->Bind(wxEVT_KILL_FOCUS, [root](wxFocusEvent& event) {
                const auto it = gButtonVisualStates.find(root);
                if (it == gButtonVisualStates.end() || !it->second.darkLike) {
                    event.Skip();
                    return;
                }
                root->SetBackgroundColour(it->second.normalBg);
                root->SetForegroundColour(it->second.text);
                root->Refresh();
                event.Skip();
            });
            root->SetBackgroundStyle(wxBG_STYLE_PAINT);
            root->Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});
            root->Bind(wxEVT_PAINT, [root](wxPaintEvent& event) {
                    const auto it = gButtonVisualStates.find(root);
                    if (it == gButtonVisualStates.end() || !it->second.darkLike) {
                        event.Skip();
                        return;
                    }
                    wxAutoBufferedPaintDC dc(root);
                    const wxRect rect = root->GetClientRect();
                    const wxColour bg = it->second.darkLike ? root->GetBackgroundColour() : it->second.normalBg;
                    const wxColour fg = it->second.text;

                    dc.SetBrush(wxBrush(bg));
                    dc.SetPen(wxPen(lightenTowardWhite(bg, 28)));
                    dc.DrawRectangle(rect);

                    if (auto* bmpBtn = dynamic_cast<wxBitmapButton*>(root)) {
                        const wxBitmap bmp = bmpBtn->GetBitmap();
                        if (bmp.IsOk()) {
                            const int x = (rect.GetWidth() - bmp.GetWidth()) / 2;
                            const int y = (rect.GetHeight() - bmp.GetHeight()) / 2;
                            dc.DrawBitmap(bmp, x, y, true);
                        }
                    } else {
                        dc.SetTextForeground(fg);
                        const wxString label = root->GetLabel();
                        dc.DrawLabel(label, rect, wxALIGN_CENTER);
                    }
                });
            root->Bind(wxEVT_DESTROY, [root](wxWindowDestroyEvent& event) {
                gButtonHoverBound.erase(root);
                gButtonVisualStates.erase(root);
                event.Skip();
            });
        }
#ifdef __WXMSW__
        if (darkLike) {
            // Disable native visual-style painting in dark mode only,
            // otherwise light theme buttons should stay fully native.
            applyNativeClassTheme(root, theme, L"", L"");
        }
#endif
    }

    if (dynamic_cast<wxStaticText*>(root) != nullptr ||
        dynamic_cast<wxStaticBitmap*>(root) != nullptr) {
        root->SetForegroundColour(palette.text);
        root->SetBackgroundColour(palette.panelBg);
        root->SetOwnForegroundColour(palette.text);
        root->SetOwnBackgroundColour(palette.panelBg);
    }

    const wxWindowList& children = root->GetChildren();
    for (wxWindowList::compatibility_iterator it = children.GetFirst(); it; it = it->GetNext()) {
        applyThemeToWindowTree(it->GetData(), palette, theme);
    }
}

}  // namespace ccm::ui
