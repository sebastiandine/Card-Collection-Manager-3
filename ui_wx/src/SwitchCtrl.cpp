#include "ccm/ui/SwitchCtrl.hpp"
#include "ccm/domain/Enums.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>

#include <algorithm>

namespace ccm::ui {

wxDEFINE_EVENT(EVT_CCM_SWITCH, wxCommandEvent);

namespace {

wxColour liftRgb(const wxColour& c, int delta) {
    auto lift = [delta](unsigned char ch) -> unsigned char {
        const int v = static_cast<int>(ch) + delta;
        return static_cast<unsigned char>(v > 255 ? 255 : (v < 0 ? 0 : v));
    };
    return wxColour(lift(c.Red()), lift(c.Green()), lift(c.Blue()));
}

}  // namespace

SwitchCtrl::SwitchCtrl(wxWindow* parent, wxWindowID id, bool initialOn)
    : wxWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE, wxString()),
      on_(initialOn) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCursor(wxCURSOR_HAND);
    const wxSize sz = FromDIP(wxSize(40, 20));
    SetMinSize(sz);
    SetMaxSize(sz);
    SetInitialSize(sz);

    Bind(wxEVT_PAINT, &SwitchCtrl::onPaint, this);
    Bind(wxEVT_LEFT_DOWN, &SwitchCtrl::onLeftDown, this);
    Bind(wxEVT_ENTER_WINDOW, &SwitchCtrl::onEnter, this);
    Bind(wxEVT_LEAVE_WINDOW, &SwitchCtrl::onLeave, this);
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});
}

void SwitchCtrl::SetValue(bool on, bool notify) {
    if (on_ == on) return;
    on_ = on;
    Refresh();
    if (notify) {
        wxCommandEvent e(EVT_CCM_SWITCH, GetId());
        e.SetEventObject(this);
        e.SetInt(on_ ? 1 : 0);
        ProcessEvent(e);
    }
}

bool SwitchCtrl::Enable(bool enable) {
    const bool ok = wxWindow::Enable(enable);
    SetCursor(enable ? wxCURSOR_HAND : wxCURSOR_ARROW);
    Refresh();
    return ok;
}

void SwitchCtrl::onEnter(wxMouseEvent& ev) {
    hovered_ = true;
    Refresh();
    ev.Skip();
}

void SwitchCtrl::onLeave(wxMouseEvent& ev) {
    hovered_ = false;
    Refresh();
    ev.Skip();
}

void SwitchCtrl::onLeftDown(wxMouseEvent& ev) {
    if (!IsEnabled()) {
        ev.Skip();
        return;
    }
    on_ = !on_;
    Refresh();
    wxCommandEvent e(EVT_CCM_SWITCH, GetId());
    e.SetEventObject(this);
    e.SetInt(on_ ? 1 : 0);
    ProcessEvent(e);
    ev.Skip(false);
}

void SwitchCtrl::onPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    const wxRect rect = GetClientRect();
    if (rect.width <= 0 || rect.height <= 0) return;

    const Theme theme = inferThemeFromWindow(this);
    const ThemePalette p = paletteForTheme(theme);
    const bool dark = theme == Theme::Dark;

    wxColour trackOff = p.inputBg;
    wxColour trackOn = p.buttonBg;
    wxColour thumb = dark ? wxColour(240, 240, 240) : wxColour(252, 252, 252);
    wxColour border = dark ? wxColour(72, 72, 72) : wxColour(158, 158, 158);

    wxColour track = on_ ? trackOn : trackOff;
    if (hovered_ && IsEnabled()) {
        track = liftRgb(track, dark ? 14 : 10);
    }
    if (!IsEnabled()) {
        track = liftRgb(track, dark ? -22 : -25);
        thumb = liftRgb(thumb, dark ? -55 : -35);
        border = liftRgb(border, dark ? -15 : 10);
    }

    // Fill the full client rect first so rounded-track corners do not show
    // undrawn pixels (often black) against the parent panel.
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(p.panelBg));
    dc.DrawRectangle(rect);

    dc.SetPen(wxPen(border));
    dc.SetBrush(wxBrush(track));
    const int radius = rect.height / 2;
    dc.DrawRoundedRectangle(rect, radius);

    const int pad = FromDIP(2);
    const int thumbD = std::max(4, rect.height - 2 * pad);
    const int travel = std::max(0, rect.width - 2 * pad - thumbD);
    const int thumbX = pad + (on_ ? travel : 0);
    const int thumbY = rect.y + (rect.height - thumbD) / 2;

    wxColour thumbBorder = liftRgb(border, dark ? 18 : -12);
    dc.SetPen(wxPen(thumbBorder));
    dc.SetBrush(wxBrush(thumb));
    dc.DrawEllipse(thumbX, thumbY, thumbD, thumbD);
}

}  // namespace ccm::ui
