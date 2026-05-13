#pragma once

#include <wx/event.h>
#include <wx/window.h>

namespace ccm::ui {

wxDECLARE_EVENT(EVT_CCM_SWITCH, wxCommandEvent);

// Small on/off switch (pill track + thumb) for modal dialogs. Fires `EVT_CCM_SWITCH`
// when the user toggles; bind with the control pointer as the event source.
class SwitchCtrl final : public wxWindow {
public:
    explicit SwitchCtrl(wxWindow* parent, wxWindowID id = wxID_ANY, bool initialOn = false);

    [[nodiscard]] bool GetValue() const noexcept { return on_; }
    void SetValue(bool on, bool notify = false);

    bool Enable(bool enable = true) override;

private:
    void onPaint(wxPaintEvent&);
    void onLeftDown(wxMouseEvent&);
    void onEnter(wxMouseEvent&);
    void onLeave(wxMouseEvent&);

    bool on_{false};
    bool hovered_{false};
};

}  // namespace ccm::ui
