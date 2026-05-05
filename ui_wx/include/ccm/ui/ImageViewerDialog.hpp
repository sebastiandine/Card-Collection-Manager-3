#pragma once

// ImageViewerDialog: full-size image viewer with prev/next navigation.

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <filesystem>
#include <vector>

namespace ccm::ui {

class ImageViewerDialog : public wxDialog {
public:
    ImageViewerDialog(wxWindow* parent,
                      std::vector<std::filesystem::path> imagePaths,
                      std::size_t startIndex);

private:
    void show(std::size_t index);
    void onPrev(wxCommandEvent&);
    void onNext(wxCommandEvent&);

    std::vector<std::filesystem::path> paths_;
    std::size_t                        index_{0};

    wxPanel*       imageHost_{nullptr};
    wxStaticText*  caption_{nullptr};
};

}  // namespace ccm::ui
