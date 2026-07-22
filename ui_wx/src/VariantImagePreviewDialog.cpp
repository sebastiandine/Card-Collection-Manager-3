#include "ccm/ui/VariantImagePreviewDialog.hpp"

#include <wx/bitmap.h>
#include <wx/dcclient.h>
#include <wx/display.h>
#include <wx/log.h>
#include <wx/mstream.h>
#include <wx/sizer.h>

#include <algorithm>

namespace ccm::ui {

wxDEFINE_EVENT(EVT_VARIANT_PREVIEW_PREV, wxCommandEvent);
wxDEFINE_EVENT(EVT_VARIANT_PREVIEW_NEXT, wxCommandEvent);

class VariantImagePreviewDialog::ImageCanvas : public wxPanel {
public:
    explicit ImageCanvas(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &ImageCanvas::onPaint, this);
        Bind(wxEVT_SIZE, [this](wxSizeEvent& ev) {
            Refresh();
            ev.Skip();
        });
    }

    void setImage(const wxImage& img) {
        original_ = img;
        cachedScaled_ = wxBitmap();
        cachedScaledFor_ = wxSize(-1, -1);
        Refresh();
    }

    void clear() {
        original_ = wxImage();
        cachedScaled_ = wxBitmap();
        cachedScaledFor_ = wxSize(-1, -1);
        Refresh();
    }

private:
    void onPaint(wxPaintEvent&) {
        wxPaintDC dc(this);
        dc.Clear();
        if (!original_.IsOk()) return;

        const wxSize ws = GetClientSize();
        if (ws.GetWidth() <= 0 || ws.GetHeight() <= 0) return;

        const double scale = std::min(
            static_cast<double>(ws.GetWidth()) / original_.GetWidth(),
            static_cast<double>(ws.GetHeight()) / original_.GetHeight());
        const int w = std::max(1, static_cast<int>(original_.GetWidth() * scale));
        const int h = std::max(1, static_cast<int>(original_.GetHeight() * scale));
        const wxSize scaledSize(w, h);
        if (!cachedScaled_.IsOk() || cachedScaledFor_ != scaledSize) {
            wxImage scaled = original_.Scale(w, h, wxIMAGE_QUALITY_HIGH);
            cachedScaled_ = wxBitmap(scaled);
            cachedScaledFor_ = scaledSize;
        }
        dc.DrawBitmap(cachedScaled_,
                      (ws.GetWidth() - w) / 2,
                      (ws.GetHeight() - h) / 2,
                      true);
    }

    wxImage  original_;
    wxBitmap cachedScaled_;
    wxSize   cachedScaledFor_{-1, -1};
};

VariantImagePreviewDialog::VariantImagePreviewDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Print preview",
               wxDefaultPosition, wxSize(280, 420),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxSTAY_ON_TOP) {
    auto* root = new wxBoxSizer(wxVERTICAL);

    imageHost_ = new ImageCanvas(this);
    root->Add(imageHost_, 1, wxEXPAND | wxALL, 6);

    caption_ = new wxStaticText(this, wxID_ANY, "");
    root->Add(caption_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    auto* nav = new wxBoxSizer(wxHORIZONTAL);
    prevButton_ = new wxButton(this, wxID_ANY, "<< Prev");
    nextButton_ = new wxButton(this, wxID_ANY, "Next >>");
    nav->Add(prevButton_, 0, wxRIGHT, 6);
    nav->Add(nextButton_, 0);
    root->Add(nav, 0, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    prevButton_->Bind(wxEVT_BUTTON, &VariantImagePreviewDialog::onPrev, this);
    nextButton_->Bind(wxEVT_BUTTON, &VariantImagePreviewDialog::onNext, this);

    SetSizer(root);
    Layout();
    repositionBesideParent();
}

void VariantImagePreviewDialog::repositionBesideParent() {
    wxWindow* parent = GetParent();
    if (parent == nullptr) return;

    const wxRect parentScreen = parent->GetScreenRect();
    const wxSize size = GetSize();
    int x = parentScreen.GetRight() + 20;
    int y = parentScreen.GetTop();

    const int displayIdx = wxDisplay::GetFromWindow(parent);
    if (displayIdx != wxNOT_FOUND) {
        const wxRect work = wxDisplay(displayIdx).GetClientArea();
        if (x + size.GetWidth() > work.GetRight()) {
            x = std::max(work.GetLeft(), work.GetRight() - size.GetWidth());
        }
        if (y + size.GetHeight() > work.GetBottom()) {
            y = std::max(work.GetTop(), work.GetBottom() - size.GetHeight());
        }
        if (x < work.GetLeft()) x = work.GetLeft();
        if (y < work.GetTop()) y = work.GetTop();
    }

    SetPosition(wxPoint(x, y));
}

void VariantImagePreviewDialog::setCaption(const wxString& caption) {
    if (caption_) {
        caption_->SetLabelText(caption);
        Layout();
    }
}

void VariantImagePreviewDialog::setImageBytes(std::string_view bytes) {
    if (!imageHost_) return;
    if (bytes.empty()) {
        clearImage();
        return;
    }

    wxMemoryInputStream stream(bytes.data(), bytes.size());
    wxImage img;
    bool decoded = false;
    {
        wxLogNull suppressPngWarnings;
        decoded = img.LoadFile(stream, wxBITMAP_TYPE_ANY);
    }
    if (!decoded || !img.IsOk()) {
        clearImage();
        return;
    }
    imageHost_->setImage(img);
}

void VariantImagePreviewDialog::clearImage() {
    if (imageHost_) {
        imageHost_->clear();
    }
}

void VariantImagePreviewDialog::setNavigationEnabled(bool enabled) {
    if (prevButton_) prevButton_->Enable(enabled);
    if (nextButton_) nextButton_->Enable(enabled);
}

void VariantImagePreviewDialog::onPrev(wxCommandEvent&) {
    wxCommandEvent ev(EVT_VARIANT_PREVIEW_PREV, GetId());
    ev.SetEventObject(this);
    ProcessWindowEvent(ev);
}

void VariantImagePreviewDialog::onNext(wxCommandEvent&) {
    wxCommandEvent ev(EVT_VARIANT_PREVIEW_NEXT, GetId());
    ev.SetEventObject(this);
    ProcessWindowEvent(ev);
}

}  // namespace ccm::ui
