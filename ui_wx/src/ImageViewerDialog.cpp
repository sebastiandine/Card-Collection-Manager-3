#include "ccm/ui/ImageViewerDialog.hpp"

#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/dcclient.h>
#include <wx/image.h>
#include <wx/panel.h>
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

    void setImage(const wxImage& img) {
        original_ = img;
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
            static_cast<double>(ws.GetWidth())  / original_.GetWidth(),
            static_cast<double>(ws.GetHeight()) / original_.GetHeight());
        const int w = std::max(1, static_cast<int>(original_.GetWidth()  * scale));
        const int h = std::max(1, static_cast<int>(original_.GetHeight() * scale));
        const wxSize scaledSize(w, h);
        if (!cachedScaled_.IsOk() || cachedScaledFor_ != scaledSize) {
            // Use normal quality for very large reductions to keep navigation snappy.
            const long long srcPixels = static_cast<long long>(original_.GetWidth()) * original_.GetHeight();
            const long long dstPixels = static_cast<long long>(w) * h;
            const bool heavyDownscale = dstPixels > 0 && srcPixels > (dstPixels * 4);
            const wxImageResizeQuality quality =
                heavyDownscale ? wxIMAGE_QUALITY_NORMAL : wxIMAGE_QUALITY_HIGH;
            wxImage scaled = original_.Scale(w, h, quality);
            cachedScaled_ = wxBitmap(scaled);
            cachedScaledFor_ = scaledSize;
        }
        dc.DrawBitmap(cachedScaled_,
                      (ws.GetWidth() - w) / 2,
                      (ws.GetHeight() - h) / 2,
                      true);
    }

    wxImage original_;
    wxBitmap cachedScaled_;
    wxSize cachedScaledFor_{-1, -1};
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
    imageCache_.resize(paths_.size());
    imageCacheReady_.assign(paths_.size(), false);

    auto* root = new wxBoxSizer(wxVERTICAL);

    imageHost_ = new ImageCanvas(this);
    root->Add(imageHost_, 1, wxEXPAND | wxALL, 6);

    caption_ = new wxStaticText(this, wxID_ANY, "");
    caption_->SetForegroundColour(*wxBLACK);
    root->Add(caption_, 0, wxALL, 6);

    auto* nav = new wxBoxSizer(wxHORIZONTAL);
    prevButton_ = new wxButton(this, wxID_ANY, "<< Prev");
    nextButton_ = new wxButton(this, wxID_ANY, "Next >>");
    auto* prev = prevButton_;
    auto* next = nextButton_;
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

bool ImageViewerDialog::loadImageAt(std::size_t index) {
    if (index >= paths_.size()) return false;
    if (imageCacheReady_[index]) return imageCache_[index].IsOk();

    wxImage img;
    if (!img.LoadFile(paths_[index].string())) {
        imageCacheReady_[index] = true;
        return false;
    }

    imageCache_[index] = std::move(img);
    imageCacheReady_[index] = true;
    return true;
}

void ImageViewerDialog::prefetchNeighbors() {
    if (paths_.size() < 2) return;
    const std::size_t prev = (index_ + paths_.size() - 1) % paths_.size();
    const std::size_t next = (index_ + 1) % paths_.size();
    loadImageAt(prev);
    loadImageAt(next);
}

void ImageViewerDialog::show(std::size_t index) {
    if (paths_.empty()) {
        caption_->SetLabelText("(no images)");
        return;
    }
    index_ = index % paths_.size();
    if (loadImageAt(index_)) static_cast<ImageCanvas*>(imageHost_)->setImage(imageCache_[index_]);
    caption_->SetLabelText(paths_[index_].filename().string() +
                           " (" + std::to_string(index_ + 1) +
                           "/" + std::to_string(paths_.size()) + ")");
    prefetchNeighbors();
    Layout();
}

void ImageViewerDialog::onPrev(wxCommandEvent&) {
    if (paths_.empty()) return;
    if (imageHost_ != nullptr) imageHost_->SetFocus();
    const std::size_t target = (index_ + paths_.size() - 1) % paths_.size();
    CallAfter([this, target]() {
        if (!IsBeingDeleted()) show(target);
    });
}

void ImageViewerDialog::onNext(wxCommandEvent&) {
    if (paths_.empty()) return;
    if (imageHost_ != nullptr) imageHost_->SetFocus();
    const std::size_t target = (index_ + 1) % paths_.size();
    CallAfter([this, target]() {
        if (!IsBeingDeleted()) show(target);
    });
}

}  // namespace ccm::ui
