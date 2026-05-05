#pragma once

// CardEditDialog: modal create/edit form for a MagicCard. Reads the cached
// set list via SetService, writes images via ImageService and only mutates the
// in-memory MagicCard; saving is the caller's responsibility (typically
// CollectionService::add / update).

#include "ccm/domain/MagicCard.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/services/SetService.hpp"

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/listbox.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>

#include <chrono>
#include <vector>

namespace ccm::ui {

enum class EditMode { Create, Edit };

class CardEditDialog : public wxDialog {
public:
    CardEditDialog(wxWindow* parent,
                   ImageService& imageService,
                   SetService& setService,
                   EditMode mode,
                   MagicCard initial,
                   const std::vector<Set>* preloadedSets = nullptr);

    // The (possibly updated) card after the user clicks OK. The id of a Create
    // result is left at 0 - CollectionService assigns the real id on add().
    [[nodiscard]] const MagicCard& card() const noexcept { return card_; }

private:
    void buildLayout();
    void populateChoices();
    void readSets();
    [[nodiscard]] const std::vector<Set>& availableSets() const noexcept;
    void writeFromControls();

    void onAddImage(wxCommandEvent&);
    void onRemoveImage(wxCommandEvent&);
    void onImageActivated(wxCommandEvent&);
    void onOk(wxCommandEvent&);

    void onSetComboChar(wxKeyEvent&);
    void onSetComboKillFocus(wxFocusEvent&);

    [[nodiscard]] bool setComboTypingSurfaceActive() const;
    void applySetTypeaheadSelection();

    ImageService& imageService_;
    SetService&   setService_;
    EditMode      mode_;
    MagicCard     card_;

    std::vector<Set> sets_;
    const std::vector<Set>* preloadedSets_{nullptr};

    wxTextCtrl* nameCtrl_{nullptr};
    // wxComboBox(wxCB_READONLY) matches native <select>-like type-to-find within the list.
    wxComboBox* setCombo_{nullptr};
    wxSpinCtrl* amountCtrl_{nullptr};
    wxChoice*   languageChoice_{nullptr};
    wxChoice*   conditionChoice_{nullptr};
    wxTextCtrl* noteCtrl_{nullptr};
    wxCheckBox* foilCheck_{nullptr};
    wxCheckBox* signedCheck_{nullptr};
    wxCheckBox* alteredCheck_{nullptr};
    wxListBox*  imagesList_{nullptr};

    /** Incremental search within the set list (native combo alone does not match HTML select behavior). */
    wxString                                       setTypeaheadPrefix_;
    std::chrono::steady_clock::time_point          setTypeaheadLastKey_{};
    static constexpr std::chrono::milliseconds kSetTypeaheadResetMs{1000};
};

}  // namespace ccm::ui
