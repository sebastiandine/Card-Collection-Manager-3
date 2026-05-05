#pragma once

// SelectedCardPanel: shows a Scryfall-fetched preview image (top) and the
// selected MagicCard's details laid out as a label/value grid. Stored card
// images are listed by index ("Image 1", "Image 2", ...) in a list box;
// double-click opens ImageViewerDialog.
//
// Emits EVT_PREVIEW_STATUS to the parent every time a preview fetch resolves:
// `event.GetString()` carries the human-readable status (empty on success,
// non-empty on failure). MainFrame uses this to surface Scryfall trouble on
// the bottom status bar without coupling the UI panel to the frame's status
// API directly.

#include "ccm/domain/MagicCard.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/event.h>
#include <wx/listbox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

namespace ccm::ui {

wxDECLARE_EVENT(EVT_PREVIEW_STATUS, wxCommandEvent);

class SelectedCardPanel : public wxPanel {
public:
    SelectedCardPanel(wxWindow* parent,
                      ImageService& imageService,
                      CardPreviewService& cardPreview);
    ~SelectedCardPanel() override;

    void setCard(std::optional<MagicCard> card);
    void applyTheme(const ThemePalette& palette);

private:
    // Shared state for the async preview fetcher. The detached worker thread
    // and any pending `CallAfter` lambdas hold a `shared_ptr` to it; we flip
    // `alive` to false in the dtor so late-arriving callbacks become no-ops.
    struct PreviewState {
        std::atomic<bool>     alive{true};
        std::atomic<unsigned> currentGen{0};
        SelectedCardPanel*    panel;
    };

    void buildInfoGrid(wxBoxSizer* root);
    void rebuildImageList();
    void onImageActivated(wxCommandEvent&);

    void clearPreview();
    void startPreviewFetch(const MagicCard& card);
    // Called on the UI thread by the worker via `wxTheApp->CallAfter`.
    void onPreviewBytes(unsigned gen, bool ok, std::string bytes, std::string err);
    // Bubble a preview status string to the parent. Empty string clears any
    // previously displayed status. Used by MainFrame to drive its status bar.
    void emitPreviewStatus(const wxString& message);

    ImageService&       imageService_;
    CardPreviewService& cardPreview_;

    std::optional<MagicCard> card_;

    wxStaticBitmap* previewBitmap_{nullptr};
    wxStaticText*   previewStatus_{nullptr};

    // Right-hand-side value cells of the info grid, set/cleared from setCard.
    wxStaticText* nameValue_{nullptr};
    wxStaticText* setValue_{nullptr};
    wxStaticText* languageValue_{nullptr};
    wxStaticText* conditionValue_{nullptr};
    wxStaticText* amountValue_{nullptr};

    // Note row hides itself when the note string is empty.
    wxStaticText* noteLabel_{nullptr};
    wxStaticText* noteValue_{nullptr};

    // Foil / Signed / Altered are rendered as a row of small SVG icons,
    // shown only when the corresponding flag is set on the card. The whole
    // row collapses when none of the flags are set. Mirrors the established
    // EntryPanelTemplate "extraAttributes" + signed/altered icon strip.
    wxStaticText*   flagsLabel_{nullptr};
    wxPanel*        flagsRow_{nullptr};
    wxStaticBitmap* foilIcon_{nullptr};
    wxStaticBitmap* signedIcon_{nullptr};
    wxStaticBitmap* alteredIcon_{nullptr};

    wxListBox* imageList_{nullptr};

    std::shared_ptr<PreviewState> state_;

    // Last MagicCard.id we *actually* kicked a preview fetch for. Lets
    // setCard() short-circuit when the same card stays selected (e.g. the
    // filter narrows but the selection stays the same), avoiding redundant
    // Scryfall calls that would trip rate limiting.
    std::optional<std::uint32_t> lastFetchedId_;
};

}  // namespace ccm::ui
