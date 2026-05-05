#include "ccm/ui/ImageViewerDialog.hpp"

#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/dcclient.h>
#include <wx/image.h>
#include <wx/sizer.h>

namespace ccm::ui {

namespace {

class ImageCanvas : public wxPanel {
public:
    explicit ImageCanvas(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &ImageCanvas::onPaint, this);
        Bind(wxEVT_SIZE,  [this](wxSizeEvent& ev) { Refresh(); ev.Skip(); });
    }

    void setImage(wxImage img) {
        original_ = std::move(img);
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
            static_cast<double>(ws.GetWidth())  / original_.GetWidth(),
            static_cast<double>(ws.GetHeight()) / original_.GetHeight());
        const int w = std::max(1, static_cast<int>(original_.GetWidth()  * scale));
        const int h = std::max(1, static_cast<int>(original_.GetHeight() * scale));
        wxImage scaled = original_.Scale(w, h, wxIMAGE_QUALITY_HIGH);
        dc.DrawBitmap(wxBitmap(scaled),
                      (ws.GetWidth() - w) / 2,
                      (ws.GetHeight() - h) / 2,
                      true);
    }

    wxImage original_;
};

}  // namespace

ImageViewerDialog::ImageViewerDialog(wxWindow* parent,
                                     std::vector<std::filesystem::path> imagePaths,
                                     std::size_t startIndex)
    : wxDialog(parent, wxID_ANY, "Image",
               wxDefaultPosition, wxSize(700, 900),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      paths_(std::move(imagePaths)),
      index_(startIndex < paths_.size() ? startIndex : 0) {
    auto* root = new wxBoxSizer(wxVERTICAL);

    imageHost_ = new ImageCanvas(this);
    root->Add(imageHost_, 1, wxEXPAND | wxALL, 6);

    caption_ = new wxStaticText(this, wxID_ANY, "");
    caption_->SetForegroundColour(*wxBLACK);
    root->Add(caption_, 0, wxALL, 6);

    auto* nav = new wxBoxSizer(wxHORIZONTAL);
    auto* prev = new wxButton(this, wxID_ANY, "<< Prev");
    auto* next = new wxButton(this, wxID_ANY, "Next >>");
    nav->Add(prev, 0, wxRIGHT, 6);
    nav->Add(next, 0);
    nav->AddStretchSpacer(1);
    nav->Add(new wxButton(this, wxID_OK, "Close"), 0);
    // Reserve bottom-right space for the dark resize-grip overlay on Windows.
    root->Add(nav, 0, wxEXPAND | wxLEFT | wxTOP | wxRIGHT, 6);
    root->AddSpacer(24);

    prev->Bind(wxEVT_BUTTON, &ImageViewerDialog::onPrev, this);
    next->Bind(wxEVT_BUTTON, &ImageViewerDialog::onNext, this);

    SetSizer(root);
    show(index_);
}

void ImageViewerDialog::show(std::size_t index) {
    if (paths_.empty()) {
        caption_->SetLabelText("(no images)");
        return;
    }
    index_ = index % paths_.size();
    wxImage img;
    if (img.LoadFile(paths_[index_].string())) {
        static_cast<ImageCanvas*>(imageHost_)->setImage(std::move(img));
    }
    caption_->SetLabelText(paths_[index_].filename().string() +
                           " (" + std::to_string(index_ + 1) +
                           "/" + std::to_string(paths_.size()) + ")");
    Layout();
}

void ImageViewerDialog::onPrev(wxCommandEvent&) {
    if (paths_.empty()) return;
    show((index_ + paths_.size() - 1) % paths_.size());
}

void ImageViewerDialog::onNext(wxCommandEvent&) {
    if (paths_.empty()) return;
    show((index_ + 1) % paths_.size());
}

}  // namespace ccm::ui
