#include "ccm/ui/SelectedCardPanel.hpp"

#include "ccm/ui/ImageViewerDialog.hpp"
#include "ccm/ui/SvgIcons.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/app.h>
#include <wx/arrstr.h>
#include <wx/bitmap.h>
#include <wx/colour.h>
#include <wx/event.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/settings.h>
#include <wx/sizer.h>

#include <algorithm>
#include <thread>
#include <utility>

namespace ccm::ui {

wxDEFINE_EVENT(EVT_PREVIEW_STATUS, wxCommandEvent);

namespace {
constexpr int kPreviewWidth   = 250;
constexpr int kPreviewHeight  = 350;  // matches legacy preview sizing (<img width=250>)
constexpr int kImageListWidth = 0;    // wx default; height controls visible rows
constexpr int kImageListHeight = 80;
constexpr int kFlagIconSize   = 14;

// Build a transparent placeholder bitmap of the preview slot size so the
// layout doesn't jump when the bitmap is cleared between fetches.
wxBitmap makePreviewPlaceholder() {
    wxImage img(kPreviewWidth, kPreviewHeight);
    img.SetAlpha();
    unsigned char* alpha = img.GetAlpha();
    if (alpha != nullptr) {
        std::fill(alpha, alpha + kPreviewWidth * kPreviewHeight, 0);
    }
    return wxBitmap(img);
}

}  // namespace

SelectedCardPanel::SelectedCardPanel(wxWindow* parent,
                                     ImageService& imageService,
                                     CardPreviewService& cardPreview)
    : wxPanel(parent, wxID_ANY),
      imageService_(imageService),
      cardPreview_(cardPreview),
      state_(std::make_shared<PreviewState>()) {
    state_->panel = this;

    auto* root = new wxBoxSizer(wxVERTICAL);

    previewBitmap_ = new wxStaticBitmap(this, wxID_ANY, makePreviewPlaceholder());
    previewStatus_ = new wxStaticText(this, wxID_ANY, "");
    root->Add(previewBitmap_, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, 6);
    root->Add(previewStatus_, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 4);

    buildInfoGrid(root);

    SetSizer(root);
    setCard(std::nullopt);
}

SelectedCardPanel::~SelectedCardPanel() {
    // Detach any in-flight worker: late `CallAfter` lambdas check this flag
    // before touching `panel` so they become no-ops after destruction.
    if (state_) {
        state_->alive.store(false);
        state_->panel = nullptr;
    }
}

void SelectedCardPanel::buildInfoGrid(wxBoxSizer* root) {
    // Two-column "label | value" form. Column 1 (the value column) grows so
    // long values get the available horizontal space.
    auto* grid = new wxFlexGridSizer(/*cols=*/2, /*vgap=*/4, /*hgap=*/12);
    grid->AddGrowableCol(1, 1);

    auto makeBoldLabel = [this](const wxString& text) {
        auto* lbl = new wxStaticText(this, wxID_ANY, text);
        wxFont lf = lbl->GetFont();
        lf.MakeBold();
        lbl->SetFont(lf);
        return lbl;
    };

    auto addRow = [grid, &makeBoldLabel](const wxString& label,
                                         wxStaticText*& valueOut) {
        auto* lbl = makeBoldLabel(label);
        valueOut = new wxStaticText(lbl->GetParent(), wxID_ANY, "");
        grid->Add(lbl, 0, wxALIGN_TOP | wxALIGN_LEFT);
        grid->Add(valueOut, 1, wxEXPAND | wxALIGN_LEFT);
    };

    addRow("Name",      nameValue_);
    addRow("Set",       setValue_);
    addRow("Language",  languageValue_);
    addRow("Condition", conditionValue_);
    addRow("Amount",    amountValue_);

    // Flags row: empty label cell, value cell holds a horizontal strip of
    // small SVG icons (foil / signed / altered). Each icon is shown only
    // when its flag is set; the whole row collapses if none are set.
    flagsLabel_ = new wxStaticText(this, wxID_ANY, "");
    flagsRow_   = new wxPanel(this, wxID_ANY);
    auto* flagsSizer = new wxBoxSizer(wxHORIZONTAL);
    const std::string textHex =
        wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)
            .GetAsString(wxC2S_HTML_SYNTAX)
            .ToStdString();
    foilIcon_    = new wxStaticBitmap(flagsRow_, wxID_ANY,
                                      svgIconBitmap(kSvgFoil,    kFlagIconSize, textHex.c_str()));
    signedIcon_  = new wxStaticBitmap(flagsRow_, wxID_ANY,
                                      svgIconBitmap(kSvgSigned,  kFlagIconSize, textHex.c_str()));
    alteredIcon_ = new wxStaticBitmap(flagsRow_, wxID_ANY,
                                      svgIconBitmap(kSvgAltered, kFlagIconSize, textHex.c_str()));
    foilIcon_->SetToolTip("Foil");
    signedIcon_->SetToolTip("Signed");
    alteredIcon_->SetToolTip("Altered");
    flagsSizer->Add(foilIcon_,    0, wxRIGHT, 6);
    flagsSizer->Add(signedIcon_,  0, wxRIGHT, 6);
    flagsSizer->Add(alteredIcon_, 0, wxRIGHT, 6);
    flagsRow_->SetSizer(flagsSizer);
    grid->Add(flagsLabel_, 0, wxALIGN_TOP | wxALIGN_LEFT);
    grid->Add(flagsRow_,   0, wxEXPAND);

    // Note row keeps a handle to its label so we can hide both cells when
    // the card has no note attached.
    noteLabel_ = makeBoldLabel("Note");
    noteValue_ = new wxStaticText(this, wxID_ANY, "");
    grid->Add(noteLabel_, 0, wxALIGN_TOP | wxALIGN_LEFT);
    grid->Add(noteValue_, 1, wxEXPAND | wxALIGN_LEFT);

    // The image list keeps its own row in the grid - left cell is the label,
    // right cell is the wxListBox itself so it lines up with the values.
    imageList_ = new wxListBox(this, wxID_ANY,
                               wxDefaultPosition,
                               wxSize(kImageListWidth, kImageListHeight),
                               0, nullptr,
                               wxLB_SINGLE);
    imageList_->Bind(wxEVT_LISTBOX_DCLICK, &SelectedCardPanel::onImageActivated, this);

    grid->Add(makeBoldLabel("Images"), 0, wxALIGN_TOP | wxALIGN_LEFT);
    grid->Add(imageList_, 1, wxEXPAND);

    root->Add(grid, 1, wxEXPAND | wxALL, 8);
}

void SelectedCardPanel::setCard(std::optional<MagicCard> card) {
    // Decide up-front whether the *previewed* card identity changed. Only
    // when it did do we restart the Scryfall fetch; otherwise we'd re-issue
    // the same network call every time setCard runs (which can happen
    // multiple times per keystroke as the filter narrows). Each fetch is two
    // HTTPS requests against api.scryfall.com — multiplying them by every
    // keystroke trips Scryfall's ~10 req/s soft rate limit.
    //
    // We compare against `lastFetchedId_` (the last id we kicked a fetch for)
    // rather than `card_` because callers may briefly toggle through nullopt
    // during a list rebuild, and we don't want that transient to count.
    const std::optional<std::uint32_t> newId =
        card ? std::optional<std::uint32_t>{card->id} : std::nullopt;
    const bool fetchTargetChanged = (newId != lastFetchedId_);

    card_ = std::move(card);

    // Show / hide the per-flag icon and collapse the whole flags row when
    // none of foil / signed / altered are set. wxFlexGridSizer collapses a
    // row whose items are all hidden, so the layout closes up cleanly.
    auto applyFlags = [this](bool foil, bool sgnd, bool altered) {
        foilIcon_->Show(foil);
        signedIcon_->Show(sgnd);
        alteredIcon_->Show(altered);
        flagsRow_->Layout();
        const bool any = foil || sgnd || altered;
        flagsLabel_->Show(any);
        flagsRow_->Show(any);
    };

    auto applyNote = [this](const std::string& note) {
        const bool has = !note.empty();
        noteValue_->SetLabelText(note);
        noteLabel_->Show(has);
        noteValue_->Show(has);
    };

    if (!card_) {
        nameValue_->SetLabelText("(no card selected)");
        setValue_->SetLabelText("");
        languageValue_->SetLabelText("");
        conditionValue_->SetLabelText("");
        amountValue_->SetLabelText("");
        applyNote("");
        applyFlags(false, false, false);
        if (fetchTargetChanged) {
            // Drop any in-flight preview by bumping the generation; otherwise
            // a previous fetch's CallAfter would still paint into our slot.
            state_->currentGen.fetch_add(1);
            lastFetchedId_.reset();
            clearPreview();
            previewStatus_->SetLabelText("");
            emitPreviewStatus("");
        }
    } else {
        const auto& c = *card_;
        nameValue_->SetLabelText(c.name);
        setValue_->SetLabelText(c.set.name);
        languageValue_->SetLabelText(std::string(to_string(c.language)));
        conditionValue_->SetLabelText(std::string(to_string(c.condition)));
        amountValue_->SetLabelText(std::to_string(c.amount));
        applyNote(c.note);
        applyFlags(c.foil, c.signed_, c.altered);
        if (fetchTargetChanged) startPreviewFetch(c);
    }
    rebuildImageList();
    Layout();
}

void SelectedCardPanel::applyTheme(const ThemePalette& palette) {
    SetBackgroundColour(palette.panelBg);
    SetForegroundColour(palette.text);
    flagsRow_->SetBackgroundColour(palette.panelBg);
    flagsRow_->SetForegroundColour(palette.text);
    imageList_->SetBackgroundColour(palette.inputBg);
    imageList_->SetForegroundColour(palette.inputText);

    const std::string textHex = palette.text.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
    foilIcon_->SetBitmap(svgIconBitmap(kSvgFoil, kFlagIconSize, textHex.c_str()));
    signedIcon_->SetBitmap(svgIconBitmap(kSvgSigned, kFlagIconSize, textHex.c_str()));
    alteredIcon_->SetBitmap(svgIconBitmap(kSvgAltered, kFlagIconSize, textHex.c_str()));
    Refresh();
}

void SelectedCardPanel::clearPreview() {
    previewBitmap_->SetBitmap(makePreviewPlaceholder());
}

void SelectedCardPanel::startPreviewFetch(const MagicCard& card) {
    clearPreview();
    previewStatus_->SetLabelText("Loading preview...");
    // Status bar gets cleared while the fetch is in flight; the result will
    // either keep it empty (success) or set the failure message below.
    emitPreviewStatus("");
    lastFetchedId_ = card.id;
    Layout();

    // Bump the generation so any previously running fetch gets discarded
    // when its CallAfter eventually runs.
    const unsigned gen = state_->currentGen.fetch_add(1) + 1;

    auto state = state_;  // shared_ptr captured by value
    CardPreviewService* svcPtr = &cardPreview_;
    const std::string name  = card.name;
    const std::string setId = card.set.id;

    std::thread([state, gen, svcPtr, name, setId]() {
        auto bytes = svcPtr->fetchPreviewBytes(Game::Magic, name, setId, "");
        const bool ok = bytes.isOk();
        std::string payload = ok ? std::move(bytes).value() : std::string{};
        std::string err     = ok ? std::string{}            : bytes.error();
        wxTheApp->CallAfter(
            [state, gen, ok, payload = std::move(payload), err = std::move(err)]() mutable {
                if (!state->alive.load()) return;
                if (state->currentGen.load() != gen) return;  // stale
                if (state->panel == nullptr) return;
                state->panel->onPreviewBytes(gen, ok, std::move(payload), std::move(err));
            });
    }).detach();
}

void SelectedCardPanel::onPreviewBytes(unsigned gen,
                                       bool ok,
                                       std::string bytes,
                                       std::string err) {
    if (gen != state_->currentGen.load()) return;  // double-check on UI thread
    if (!ok || bytes.empty()) {
        previewStatus_->SetLabelText("(no preview available)");
        clearPreview();
        Layout();
        // Surface the underlying reason on the bottom status bar. CCM3's
        // preview source layers HTTP errors ("HTTP 429 from ..."), URL/parse
        // errors, and Scryfall semantic failures ("Scryfall returned no
        // matching cards.") through the same Result<> channel — keep the
        // message intact so the user can see whether it's connectivity, a
        // missing card, or rate limiting. Empty err falls back to a generic
        // line so we never emit just "Preview unavailable: ".
        wxString detail = err.empty()
            ? wxString("no preview returned")
            : wxString::FromUTF8(err);
        emitPreviewStatus("Preview unavailable: " + detail);
        return;
    }

    wxMemoryInputStream stream(bytes.data(), bytes.size());
    wxImage img;
    if (!img.LoadFile(stream, wxBITMAP_TYPE_ANY)) {
        previewStatus_->SetLabelText("(preview decode failed)");
        clearPreview();
        Layout();
        emitPreviewStatus("Preview unavailable: image decode failed");
        return;
    }
    if (img.GetWidth() != kPreviewWidth || img.GetHeight() != kPreviewHeight) {
        img.Rescale(kPreviewWidth, kPreviewHeight, wxIMAGE_QUALITY_HIGH);
    }
    previewBitmap_->SetBitmap(wxBitmap(img));
    previewStatus_->SetLabelText("");
    Layout();
    emitPreviewStatus("");
}

void SelectedCardPanel::emitPreviewStatus(const wxString& message) {
    wxCommandEvent ev(EVT_PREVIEW_STATUS, GetId());
    ev.SetEventObject(this);
    ev.SetString(message);
    if (auto* parent = GetParent()) {
        parent->GetEventHandler()->ProcessEvent(ev);
    } else {
        ProcessWindowEvent(ev);
    }
}

void SelectedCardPanel::rebuildImageList() {
    imageList_->Clear();
    if (!card_) return;
    for (std::size_t i = 0; i < card_->images.size(); ++i) {
        imageList_->Append(wxString::Format("Image %zu", i + 1));
    }
}

void SelectedCardPanel::onImageActivated(wxCommandEvent& event) {
    if (!card_) return;
    const int sel = event.GetSelection();
    if (sel < 0 || static_cast<std::size_t>(sel) >= card_->images.size()) return;

    std::vector<std::filesystem::path> paths;
    paths.reserve(card_->images.size());
    for (const auto& name : card_->images) {
        paths.push_back(imageService_.resolveImagePath(Game::Magic, name));
    }
    ImageViewerDialog dlg(this, std::move(paths), static_cast<std::size_t>(sel));
    const Theme theme = inferThemeFromWindow(this);
    applyThemeToWindowTree(&dlg, paletteForTheme(theme), theme);
    dlg.ShowModal();
}

}  // namespace ccm::ui
