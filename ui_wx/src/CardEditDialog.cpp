#include "ccm/ui/CardEditDialog.hpp"

#include "ccm/domain/Enums.hpp"
#include "ccm/ui/ImageViewerDialog.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/arrstr.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#ifdef __WXMSW__
#include <wx/msw/wrapwin.h>
#endif

#include <algorithm>
#include <cctype>

namespace ccm::ui {

CardEditDialog::CardEditDialog(wxWindow* parent,
                               ImageService& imageService,
                               SetService& setService,
                               EditMode mode,
                               MagicCard initial,
                               const std::vector<Set>* preloadedSets)
    : wxDialog(parent, wxID_ANY,
               mode == EditMode::Create ? "Add Magic Card" : "Edit Magic Card",
               wxDefaultPosition, wxSize(560, 540),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      imageService_(imageService),
      setService_(setService),
      mode_(mode),
      card_(std::move(initial)),
      preloadedSets_(preloadedSets) {
    Freeze();
    if (preloadedSets_ == nullptr) {
        readSets();
    }
    buildLayout();
    populateChoices();
    Thaw();
}

void CardEditDialog::readSets() {
    auto loaded = setService_.getSets(Game::Magic);
    if (loaded.isOk()) {
        sets_ = std::move(loaded).value();
    }
}

const std::vector<Set>& CardEditDialog::availableSets() const noexcept {
    if (preloadedSets_ != nullptr) {
        return *preloadedSets_;
    }
    return sets_;
}

void CardEditDialog::buildLayout() {
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* grid = new wxFlexGridSizer(2, 6, 8);
    grid->AddGrowableCol(1, 1);

    auto addRow = [&](const wxString& label, wxWindow* ctrl) {
        grid->Add(new wxStaticText(this, wxID_ANY, label),
                  0, wxALIGN_CENTER_VERTICAL);
        grid->Add(ctrl, 1, wxEXPAND);
    };

    nameCtrl_  = new wxTextCtrl(this, wxID_ANY, card_.name);
    addRow("Name", nameCtrl_);

    // Read-only combo: native dropdown-list semantics (type letters to jump), like an HTML <select>.
    setCombo_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0,
                               nullptr, wxCB_READONLY);
    addRow("Set", setCombo_);

    amountCtrl_ = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition,
                                 wxDefaultSize, wxSP_ARROW_KEYS, 1, 255, card_.amount);
    addRow("Amount", amountCtrl_);

    languageChoice_ = new wxChoice(this, wxID_ANY);
    addRow("Language", languageChoice_);

    conditionChoice_ = new wxChoice(this, wxID_ANY);
    addRow("Condition", conditionChoice_);

    noteCtrl_ = new wxTextCtrl(this, wxID_ANY, card_.note,
                               wxDefaultPosition, wxSize(-1, 60), wxTE_MULTILINE);
    addRow("Note", noteCtrl_);

    foilCheck_    = new wxCheckBox(this, wxID_ANY, "Foil");
    signedCheck_  = new wxCheckBox(this, wxID_ANY, "Signed");
    alteredCheck_ = new wxCheckBox(this, wxID_ANY, "Altered");
    foilCheck_->SetValue(card_.foil);
    signedCheck_->SetValue(card_.signed_);
    alteredCheck_->SetValue(card_.altered);
    auto* flagsBox = new wxBoxSizer(wxHORIZONTAL);
    flagsBox->Add(foilCheck_,    0, wxRIGHT, 12);
    flagsBox->Add(signedCheck_,  0, wxRIGHT, 12);
    flagsBox->Add(alteredCheck_, 0, wxRIGHT, 12);
    grid->Add(new wxStaticText(this, wxID_ANY, "Flags"),
              0, wxALIGN_CENTER_VERTICAL);
    grid->Add(flagsBox, 1, wxEXPAND);

    root->Add(grid, 0, wxALL | wxEXPAND, 10);

    // Image management.
    auto* imgBox = new wxStaticBoxSizer(wxVERTICAL, this, "Images");
    imagesList_ = new wxListBox(this, wxID_ANY);
    for (const auto& name : card_.images) imagesList_->Append(name);
    imgBox->Add(imagesList_, 1, wxEXPAND | wxALL, 4);

    auto* imgButtons = new wxBoxSizer(wxHORIZONTAL);
    auto* addBtn = new wxButton(this, wxID_ANY, "Add image...");
    auto* rmBtn  = new wxButton(this, wxID_ANY, "Remove image");
    imgButtons->Add(addBtn, 0, wxRIGHT, 6);
    imgButtons->Add(rmBtn,  0);
    imgBox->Add(imgButtons, 0, wxALL, 4);
    root->Add(imgBox, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

    addBtn->Bind(wxEVT_BUTTON, &CardEditDialog::onAddImage,    this);
    rmBtn->Bind (wxEVT_BUTTON, &CardEditDialog::onRemoveImage, this);
    imagesList_->Bind(wxEVT_LISTBOX_DCLICK, &CardEditDialog::onImageActivated, this);

    auto* btns = CreateButtonSizer(wxOK | wxCANCEL);
    if (btns) {
        // Reserve bottom-right space for the dark resize-grip overlay on Windows.
        root->Add(btns, 0, wxLEFT | wxTOP | wxRIGHT | wxEXPAND, 10);
        root->AddSpacer(24);
    }

    Bind(wxEVT_BUTTON, &CardEditDialog::onOk, this, wxID_OK);

    setCombo_->Bind(wxEVT_CHAR, &CardEditDialog::onSetComboChar, this);
    setCombo_->Bind(wxEVT_KILL_FOCUS, &CardEditDialog::onSetComboKillFocus, this);

    SetSizer(root);

    // Keep long text fields left-aligned on first paint.
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

void CardEditDialog::populateChoices() {
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
        setNames.Add(available[i].name);
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
        setCombo_->Append("(no sets cached - use Sets > Update Magic)");
        setCombo_->SetSelection(0);
        setCombo_->Disable();
    }

    languageChoice_->Clear();
    int langIdx = 0;
    int i = 0;
    wxArrayString langs;
    langs.Alloc(allLanguages().size());
    for (auto l : allLanguages()) {
        langs.Add(std::string(to_string(l)));
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
        conditions.Add(std::string(to_string(c)));
        if (c == card_.condition) condIdx = i;
        ++i;
    }
    if (!conditions.empty()) {
        conditionChoice_->Append(conditions);
    }
    conditionChoice_->SetSelection(condIdx);
}

void CardEditDialog::writeFromControls() {
    const auto& available = availableSets();
    card_.name      = nameCtrl_->GetValue().ToStdString();
    card_.amount    = static_cast<std::uint8_t>(amountCtrl_->GetValue());
    card_.note      = noteCtrl_->GetValue().ToStdString();
    card_.foil      = foilCheck_->IsChecked();
    card_.signed_   = signedCheck_->IsChecked();
    card_.altered   = alteredCheck_->IsChecked();

    if (!available.empty() && setCombo_->IsEnabled()) {
        const int sel = setCombo_->GetSelection();
        if (sel >= 0 && static_cast<std::size_t>(sel) < available.size()) {
            card_.set = available[static_cast<std::size_t>(sel)];
        }
    }
    if (auto l = languageFromString(languageChoice_->GetStringSelection().ToStdString())) {
        card_.language = *l;
    }
    if (auto c = conditionFromString(conditionChoice_->GetStringSelection().ToStdString())) {
        card_.condition = *c;
    }
}

void CardEditDialog::onAddImage(wxCommandEvent&) {
    wxFileDialog dlg(this, "Choose image(s)",
                     wxEmptyString, wxEmptyString,
                     "Image files (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
    if (dlg.ShowModal() != wxID_OK) return;

    writeFromControls();  // ensure name/set are current for filename build
    if (card_.name.empty() || card_.set.id.empty()) {
        wxMessageBox("Set the card name and set before adding images.",
                     "Add image", wxOK | wxICON_INFORMATION, this);
        return;
    }
    wxArrayString paths;
    dlg.GetPaths(paths);

    std::vector<std::string> failed;
    failed.reserve(static_cast<std::size_t>(paths.size()));

    for (const auto& path : paths) {
        auto added = imageService_.addImage(Game::Magic,
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
        wxMessageBox(msg, "Add image", wxOK | wxICON_WARNING, this);
    }
}

void CardEditDialog::onRemoveImage(wxCommandEvent&) {
    const int sel = imagesList_->GetSelection();
    if (sel == wxNOT_FOUND) return;
    const std::string name = imagesList_->GetString(sel).ToStdString();
    auto rm = imageService_.removeImage(Game::Magic, name);
    if (!rm) {
        wxMessageBox("Failed to remove image: " + rm.error(),
                     "Error", wxOK | wxICON_ERROR, this);
        return;
    }
    card_.images.erase(card_.images.begin() + sel);
    imagesList_->Delete(static_cast<unsigned int>(sel));
}

void CardEditDialog::onImageActivated(wxCommandEvent& event) {
    const int sel = event.GetSelection();
    if (sel < 0 || static_cast<std::size_t>(sel) >= card_.images.size()) return;

    std::vector<std::filesystem::path> paths;
    paths.reserve(card_.images.size());
    for (const auto& name : card_.images) {
        paths.push_back(imageService_.resolveImagePath(Game::Magic, name));
    }

    ImageViewerDialog dlg(this, std::move(paths), static_cast<std::size_t>(sel));
    const Theme theme = inferThemeFromWindow(this);
    applyThemeToWindowTree(&dlg, paletteForTheme(theme), theme);
    dlg.ShowModal();
}

void CardEditDialog::onOk(wxCommandEvent& ev) {
    writeFromControls();
    if (card_.name.empty()) {
        wxMessageBox("Name is required.", "Add card",
                     wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (card_.set.id.empty()) {
        wxMessageBox("Pick a set first (use Sets > Update Magic if the list is empty).",
                     "Add card", wxOK | wxICON_INFORMATION, this);
        return;
    }
    ev.Skip();  // let wx finish closing the dialog with wxID_OK
}

bool CardEditDialog::setComboTypingSurfaceActive() const {
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
    // While the dropdown list is open, focus often belongs to the popup list HWND,
    // which is not parented under wxComboBox — still treat keystrokes as search.
    static constexpr UINT kCbGetDroppedState = 0x0157;  // CB_GETDROPPEDSTATE
    WXHWND       wxh = setCombo_->GetHandle();
    const HWND   h   = reinterpret_cast<HWND>(wxh);
    return h != nullptr && ::SendMessageW(h, kCbGetDroppedState, 0, 0) != 0;
#else
    return false;
#endif
}

void CardEditDialog::applySetTypeaheadSelection() {
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

void CardEditDialog::onSetComboChar(wxKeyEvent& ev) {
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

void CardEditDialog::onSetComboKillFocus(wxFocusEvent& ev) {
    // Do not clear on transient focus hops (e.g. combo popup on Windows).
    // Timeout-based reset remains the primary behavior.
    if (!setComboTypingSurfaceActive()) {
        setTypeaheadPrefix_.clear();
    }
    ev.Skip();
}

}  // namespace ccm::ui
