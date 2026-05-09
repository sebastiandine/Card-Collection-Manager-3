#pragma once

// BaseCardEditDialog<TCard>
//
// Header-only template for the modal create/edit form. Owns the parts every
// game shares — Name, Set picker (read-only combo with prefix typeahead),
// Amount spin, Language and Condition choices, Note, Image list with
// Add/Remove/double-click-to-view, OK/Cancel — and exposes hooks the
// subclass uses to:
//
//   - declare a flags row (`Foil` for Magic, `Holo` + `1. Edition` for Pokemon, ...)
//   - declare any extra game-specific text fields (`Set #` for Pokemon)
//   - read/write the typed `TCard`
//
// New games extend this template — see `MagicCardEditDialog` and
// `PokemonCardEditDialog` for the canonical patterns.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/services/SetService.hpp"
#include "ccm/ui/ImageViewerDialog.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/strconv.h>
#include <wx/textctrl.h>

#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace ccm::ui {

enum class EditMode { Create, Edit };

template <typename TCard>
class BaseCardEditDialog : public wxDialog {
public:
    using card_type = TCard;

    [[nodiscard]] const TCard& card() const noexcept { return card_; }

protected:
    BaseCardEditDialog(wxWindow* parent,
                       const wxString& title,
                       ImageService& imageService,
                       SetService& setService,
                       EditMode mode,
                       TCard initial,
                       Game game,
                       const std::vector<Set>* preloadedSets = nullptr)
        : wxDialog(parent, wxID_ANY, title,
                   wxDefaultPosition, wxSize(560, 540),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          imageService_(imageService),
          setService_(setService),
          mode_(mode),
          card_(std::move(initial)),
          game_(game),
          preloadedSets_(preloadedSets) {}

    // Subclass calls this from its constructor body once it can answer the
    // virtual hooks below.
    void buildAndPopulate() {
        Freeze();
        if (preloadedSets_ == nullptr) {
            readSets();
        }
        buildLayout();
        populateChoices();
        Thaw();
    }

    // Hooks -------------------------------------------------------------------

    // Subclass appends its game-specific check boxes / inputs onto `flagsBox`
    // (a horizontal `wxBoxSizer`). Build and bind the widgets the subclass
    // wants; the base only owns the surrounding label.
    virtual void buildFlagsRow(wxBoxSizer* flagsBox) = 0;

    // Subclass adds any extra game-specific labelled rows just below the
    // standard rows but above the Note row, by calling `appendRow(label, ctrl)`
    // (provided as a parameter). Default does nothing.
    using AppendRowFn = void (*)(BaseCardEditDialog*, const wxString&, wxWindow*);
    virtual void appendExtraRows(wxFlexGridSizer* /*grid*/) {}

    // Subclass copies the extra fields it owns from `card_` into its widgets.
    virtual void readExtraFromCard() {}

    // Subclass copies the extra fields it owns from its widgets back into `card_`.
    virtual void writeExtraToCard() {}

    [[nodiscard]] virtual std::string updateMenuName() const { return "Update Sets"; }

    // Display name passed into errors and the dialog title hints.
    [[nodiscard]] virtual std::string emptySetMessage() const {
        return "(no sets cached - use Sets > " + updateMenuName() + ")";
    }

    // Common helpers ----------------------------------------------------------

    void appendRow(wxFlexGridSizer* grid, const wxString& label, wxWindow* ctrl) {
        grid->Add(new wxStaticText(this, wxID_ANY, label),
                  0, wxALIGN_CENTER_VERTICAL);
        grid->Add(ctrl, 1, wxEXPAND);
    }

    [[nodiscard]] TCard&       mutableCard() noexcept       { return card_; }
    [[nodiscard]] const TCard& constCard() const noexcept   { return card_; }
    void syncCardFromControls() { writeFromControls(); }
    [[nodiscard]] wxComboBox* setComboControl() const noexcept { return setCombo_; }

    [[nodiscard]] const Set* selectedSetFromControls() const {
        const auto& available = availableSets();
        if (!setCombo_ || !setCombo_->IsEnabled()) return nullptr;
        const int sel = setCombo_->GetSelection();
        if (sel < 0 || static_cast<std::size_t>(sel) >= available.size()) return nullptr;
        return &available[static_cast<std::size_t>(sel)];
    }

private:
    void readSets() {
        auto loaded = setService_.getSets(game_);
        if (loaded.isOk()) {
            sets_ = std::move(loaded).value();
            std::sort(sets_.begin(), sets_.end(),
                      [](const Set& a, const Set& b) {
                          return a.releaseDate < b.releaseDate;
                      });
        }
    }

    [[nodiscard]] const std::vector<Set>& availableSets() const noexcept {
        return preloadedSets_ != nullptr ? *preloadedSets_ : sets_;
    }

    void buildLayout() {
        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* grid = new wxFlexGridSizer(2, 6, 8);
        grid->AddGrowableCol(1, 1);

        nameCtrl_  = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(card_.name.c_str()));
        appendRow(grid, "Name", nameCtrl_);

        setCombo_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0,
                                   nullptr, wxCB_READONLY);
        appendRow(grid, "Set", setCombo_);

        // Subclass extra rows go between Set and Amount (Pokemon adds Set #).
        appendExtraRows(grid);

        amountCtrl_ = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition,
                                     wxDefaultSize, wxSP_ARROW_KEYS, 1, 255, card_.amount);
        appendRow(grid, "Amount", amountCtrl_);

        languageChoice_ = new wxChoice(this, wxID_ANY);
        appendRow(grid, "Language", languageChoice_);

        conditionChoice_ = new wxChoice(this, wxID_ANY);
        appendRow(grid, "Condition", conditionChoice_);

        noteCtrl_ = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(card_.note.c_str()),
                                   wxDefaultPosition, wxSize(-1, 60), wxTE_MULTILINE);
        appendRow(grid, "Note", noteCtrl_);

        auto* flagsBox = new wxBoxSizer(wxHORIZONTAL);
        buildFlagsRow(flagsBox);
        grid->Add(new wxStaticText(this, wxID_ANY, "Flags"),
                  0, wxALIGN_CENTER_VERTICAL);
        grid->Add(flagsBox, 1, wxEXPAND);

        root->Add(grid, 0, wxALL | wxEXPAND, 10);

        auto* imgBox = new wxStaticBoxSizer(wxVERTICAL, this, "Images");
        imagesList_ = new wxListBox(this, wxID_ANY);
        for (const auto& name : card_.images) imagesList_->Append(wxString::FromUTF8(name.c_str()));
        imgBox->Add(imagesList_, 1, wxEXPAND | wxALL, 4);

        auto* imgButtons = new wxBoxSizer(wxHORIZONTAL);
        auto* addBtn = new wxButton(this, wxID_ANY, "Add image...");
        auto* rmBtn  = new wxButton(this, wxID_ANY, "Remove image");
        imgButtons->Add(addBtn, 0, wxRIGHT, 6);
        imgButtons->Add(rmBtn,  0);
        imgBox->Add(imgButtons, 0, wxALL, 4);
        root->Add(imgBox, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

        addBtn->Bind(wxEVT_BUTTON, &BaseCardEditDialog::onAddImage,    this);
        rmBtn->Bind (wxEVT_BUTTON, &BaseCardEditDialog::onRemoveImage, this);
        imagesList_->Bind(wxEVT_LISTBOX_DCLICK, &BaseCardEditDialog::onImageActivated, this);

        auto* btns = CreateButtonSizer(wxOK | wxCANCEL);
        if (btns) {
            root->Add(btns, 0, wxLEFT | wxTOP | wxRIGHT | wxEXPAND, 10);
            root->AddSpacer(24);
        }

        Bind(wxEVT_BUTTON, &BaseCardEditDialog::onOk, this, wxID_OK);

        setCombo_->Bind(wxEVT_CHAR, &BaseCardEditDialog::onSetComboChar, this);
        setCombo_->Bind(wxEVT_KILL_FOCUS, &BaseCardEditDialog::onSetComboKillFocus, this);

        SetSizer(root);

        readExtraFromCard();

        CallAfter([this]() {
            if (nameCtrl_) {
                nameCtrl_->SetInsertionPoint(0);
                nameCtrl_->ShowPosition(0);
            }
            if (noteCtrl_) {
                noteCtrl_->SetInsertionPoint(0);
                noteCtrl_->ShowPosition(0);
            }
        });
    }

    void populateChoices() {
        const auto& available = availableSets();
        setTypeaheadPrefix_.clear();
        setCombo_->Clear();
        auto lowerAscii = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            return s;
        };
        const std::string selectedSetId = lowerAscii(card_.set.id);
        int selectIdx = wxNOT_FOUND;
        wxArrayString setNames;
        setNames.Alloc(available.size());
        for (std::size_t i = 0; i < available.size(); ++i) {
            setNames.Add(wxString::FromUTF8(available[i].name.c_str()));
            if (!selectedSetId.empty() && lowerAscii(available[i].id) == selectedSetId) {
                selectIdx = static_cast<int>(i);
            }
        }
        if (!setNames.empty()) {
            setCombo_->Append(setNames);
        }
        if (selectIdx == wxNOT_FOUND && !available.empty()) selectIdx = 0;
        if (selectIdx != wxNOT_FOUND) setCombo_->SetSelection(selectIdx);
        if (available.empty()) {
            setCombo_->Append(emptySetMessage());
            setCombo_->SetSelection(0);
            setCombo_->Disable();
        }

        languageChoice_->Clear();
        int langIdx = 0;
        int i = 0;
        wxArrayString langs;
        langs.Alloc(allLanguages().size());
        for (auto l : allLanguages()) {
            const std::string lang = std::string(to_string(l));
            langs.Add(wxString::FromUTF8(lang.c_str()));
            if (l == card_.language) langIdx = i;
            ++i;
        }
        if (!langs.empty()) {
            languageChoice_->Append(langs);
        }
        languageChoice_->SetSelection(langIdx);

        conditionChoice_->Clear();
        int condIdx = 0;
        i = 0;
        wxArrayString conditions;
        conditions.Alloc(allConditions().size());
        for (auto c : allConditions()) {
            const std::string cond = std::string(to_string(c));
            conditions.Add(wxString::FromUTF8(cond.c_str()));
            if (c == card_.condition) condIdx = i;
            ++i;
        }
        if (!conditions.empty()) {
            conditionChoice_->Append(conditions);
        }
        conditionChoice_->SetSelection(condIdx);
    }

    void writeFromControls() {
        const auto& available = availableSets();
        card_.name      = nameCtrl_->GetValue().ToStdString(wxConvUTF8);
        card_.amount    = static_cast<std::uint8_t>(amountCtrl_->GetValue());
        card_.note      = noteCtrl_->GetValue().ToStdString(wxConvUTF8);

        if (!available.empty() && setCombo_->IsEnabled()) {
            const int sel = setCombo_->GetSelection();
            if (sel >= 0 && static_cast<std::size_t>(sel) < available.size()) {
                card_.set = available[static_cast<std::size_t>(sel)];
            }
        }
        if (auto l = languageFromString(languageChoice_->GetStringSelection().ToStdString(wxConvUTF8))) {
            card_.language = *l;
        }
        if (auto c = conditionFromString(conditionChoice_->GetStringSelection().ToStdString(wxConvUTF8))) {
            card_.condition = *c;
        }

        writeExtraToCard();
    }

    void onAddImage(wxCommandEvent&) {
        wxFileDialog dlg(this, "Choose image(s)",
                         wxEmptyString, wxEmptyString,
                         "Image files (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg",
                         wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
        if (dlg.ShowModal() != wxID_OK) return;

        writeFromControls();
        if (card_.name.empty() || card_.set.id.empty()) {
            showThemedMessageDialog(this, "Set the card name and set before adding images.",
                                    "Add image", wxOK | wxICON_INFORMATION);
            return;
        }
        wxArrayString paths;
        dlg.GetPaths(paths);

        std::vector<std::string> failed;
        failed.reserve(static_cast<std::size_t>(paths.size()));

        for (const auto& path : paths) {
            auto added = imageService_.addImage(game_,
                                                std::filesystem::path(path.ToStdString()),
                                                mode_ == EditMode::Create,
                                                card_.id,
                                                card_.set.name,
                                                card_.name,
                                                card_.images);
            if (!added) {
                failed.push_back(path.ToStdString() + " (" + added.error() + ")");
                continue;
            }
            card_.images.push_back(added.value());
            imagesList_->Append(added.value());
        }

        if (!failed.empty()) {
            std::string msg = "Some images could not be added:\n\n";
            for (const auto& err : failed) {
                msg += "- " + err + '\n';
            }
            showThemedMessageDialog(this, msg, "Add image", wxOK | wxICON_WARNING);
        }
    }

    void onRemoveImage(wxCommandEvent&) {
        const int sel = imagesList_->GetSelection();
        if (sel == wxNOT_FOUND) return;
        const std::string name = imagesList_->GetString(sel).ToStdString(wxConvUTF8);
        auto rm = imageService_.removeImage(game_, name);
        if (!rm) {
            showThemedMessageDialog(this, "Failed to remove image: " + rm.error(),
                                    "Error", wxOK | wxICON_ERROR);
            return;
        }
        card_.images.erase(card_.images.begin() + sel);
        imagesList_->Delete(static_cast<unsigned int>(sel));
    }

    void onImageActivated(wxCommandEvent& event) {
        const int sel = event.GetSelection();
        if (sel < 0 || static_cast<std::size_t>(sel) >= card_.images.size()) return;

        std::vector<std::filesystem::path> paths;
        paths.reserve(card_.images.size());
        for (const auto& name : card_.images) {
            paths.push_back(imageService_.resolveImagePath(game_, name));
        }
        ImageViewerDialog dlg(this, std::move(paths), static_cast<std::size_t>(sel));
        const Theme theme = inferThemeFromWindow(this);
        applyThemeToWindowTree(&dlg, paletteForTheme(theme), theme);
        dlg.ShowModal();
    }

    void onOk(wxCommandEvent& ev) {
        writeFromControls();
        if (card_.name.empty()) {
            showThemedMessageDialog(this, "Name is required.", "Add card",
                                    wxOK | wxICON_INFORMATION);
            return;
        }
        if (card_.set.id.empty()) {
            showThemedMessageDialog(this, "Pick a set first (use Sets > " + updateMenuName() + " if the list is empty).",
                                    "Add card", wxOK | wxICON_INFORMATION);
            return;
        }
        ev.Skip();
    }

    [[nodiscard]] bool setComboTypingSurfaceActive() const {
        wxWindow* focus = wxWindow::FindFocus();
        if (!setCombo_) return false;

        auto enclosedBy = [](wxWindow* root, wxWindow* leaf) -> bool {
            if (!root || !leaf) return false;
            for (wxWindow* w = leaf; w != nullptr; w = w->GetParent()) {
                if (w == root) return true;
            }
            return false;
        };

        if (enclosedBy(setCombo_, focus)) return true;

#ifdef __WXMSW__
        static constexpr UINT kCbGetDroppedState = 0x0157;  // CB_GETDROPPEDSTATE
        WXHWND       wxh = setCombo_->GetHandle();
        const HWND   h   = reinterpret_cast<HWND>(wxh);
        return h != nullptr && ::SendMessageW(h, kCbGetDroppedState, 0, 0) != 0;
#else
        return false;
#endif
    }

    void applySetTypeaheadSelection() {
        const auto& available = availableSets();
        if (!setCombo_ || available.empty()) return;
        wxString pref = setTypeaheadPrefix_;
        pref.MakeLower();
        if (pref.empty()) return;
        for (std::size_t i = 0; i < available.size(); ++i) {
            wxString name(wxString::FromUTF8(available[i].name));
            name.MakeLower();
            if (name.StartsWith(pref)) {
                setCombo_->SetSelection(static_cast<int>(i));
                return;
            }
        }
    }

    void onSetComboChar(wxKeyEvent& ev) {
        if (!setCombo_->IsEnabled() || availableSets().empty()) {
            ev.Skip();
            return;
        }

        const int mods = ev.GetModifiers();
        if ((mods & (wxMOD_CONTROL | wxMOD_ALT | wxMOD_META)) != 0) {
            ev.Skip();
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (!setTypeaheadPrefix_.empty() &&
            now - setTypeaheadLastKey_ > kSetTypeaheadResetMs) {
            setTypeaheadPrefix_.clear();
        }
        setTypeaheadLastKey_ = now;

        const int code = ev.GetKeyCode();

        if (code == WXK_BACK) {
            if (!setTypeaheadPrefix_.empty())
                setTypeaheadPrefix_.RemoveLast();
            applySetTypeaheadSelection();
            ev.Skip(false);
            return;
        }

        if (code == WXK_TAB || code == WXK_RETURN || code == WXK_ESCAPE ||
            code == WXK_UP || code == WXK_DOWN || code == WXK_LEFT || code == WXK_RIGHT ||
            code == WXK_HOME || code == WXK_END || code == WXK_PAGEUP || code == WXK_PAGEDOWN ||
            code == WXK_NUMPAD_ENTER || code == WXK_INSERT || code == WXK_DELETE ||
            code == WXK_F4 || (code >= WXK_F1 && code <= WXK_F24)) {
            ev.Skip();
            return;
        }

        wxChar uc = static_cast<wxChar>(ev.GetUnicodeKey());
        if (uc == WXK_NONE && code == WXK_SPACE)
            uc = wxT(' ');
        if (uc == WXK_NONE && code >= 32 && code < 127)
            uc = static_cast<wxChar>(code);

        if (uc == WXK_NONE || static_cast<unsigned>(uc) < 32u) {
            ev.Skip();
            return;
        }

        wxString chunk(uc);
        chunk.MakeLower();
        setTypeaheadPrefix_ += chunk;
        applySetTypeaheadSelection();
        ev.Skip(false);
    }

    void onSetComboKillFocus(wxFocusEvent& ev) {
        if (!setComboTypingSurfaceActive()) {
            setTypeaheadPrefix_.clear();
        }
        ev.Skip();
    }

    ImageService& imageService_;
    SetService&   setService_;
    EditMode      mode_;
    TCard         card_;
    Game          game_;

    std::vector<Set>        sets_;
    const std::vector<Set>* preloadedSets_{nullptr};

    wxTextCtrl* nameCtrl_{nullptr};
    wxComboBox* setCombo_{nullptr};
    wxSpinCtrl* amountCtrl_{nullptr};
    wxChoice*   languageChoice_{nullptr};
    wxChoice*   conditionChoice_{nullptr};
    wxTextCtrl* noteCtrl_{nullptr};
    wxListBox*  imagesList_{nullptr};

    wxString                                       setTypeaheadPrefix_;
    std::chrono::steady_clock::time_point          setTypeaheadLastKey_{};
    static constexpr std::chrono::milliseconds kSetTypeaheadResetMs{1000};
};

}  // namespace ccm::ui
