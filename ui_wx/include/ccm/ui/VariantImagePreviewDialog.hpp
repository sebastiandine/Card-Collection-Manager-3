#pragma once

// VariantImagePreviewDialog: small modeless popup that shows a single card
// preview image (bytes decoded as wxImage). Used by Japanese Pokémon Add/Edit
// when cycling UnnumberedPromo prints. Prev/Next fire custom events so the
// edit dialog owns the variant ring.

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/image.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/string.h>

#include <string_view>

namespace ccm::ui {

wxDECLARE_EVENT(EVT_VARIANT_PREVIEW_PREV, wxCommandEvent);
wxDECLARE_EVENT(EVT_VARIANT_PREVIEW_NEXT, wxCommandEvent);

class VariantImagePreviewDialog : public wxDialog {
public:
    explicit VariantImagePreviewDialog(wxWindow* parent);

    void setCaption(const wxString& caption);
    void setImageBytes(std::string_view bytes);
    void clearImage();
    void setNavigationEnabled(bool enabled);
    void repositionBesideParent();

private:
    class ImageCanvas;

    void onPrev(wxCommandEvent&);
    void onNext(wxCommandEvent&);

    ImageCanvas*  imageHost_{nullptr};
    wxStaticText* caption_{nullptr};
    wxButton*     prevButton_{nullptr};
    wxButton*     nextButton_{nullptr};
};

}  // namespace ccm::ui
