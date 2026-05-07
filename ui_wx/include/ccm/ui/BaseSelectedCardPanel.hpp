#pragma once

// BaseSelectedCardPanel<TCard>
//
// Header-only template for the right-hand-side card detail panel:
//
//   - top: external preview image fetched by `CardPreviewService`
//   - middle: 2-column "label | value" detail grid (Name / Set / ...)
//   - flag-icon row (collapses to nothing when no flags are set)
//   - bottom: "Image N" list box with double-click viewer
//
// All of the threading/cancellation machinery for the preview fetch is here
// (the `shared_ptr<State>` + `std::atomic alive` / `currentGen` pattern from
// `ui_wx/AGENTS.md`). Subclasses just describe which detail rows to show, the
// flag-icon strip, and how to extract `(name, setId, setNo)` for the preview
// lookup key.
//
// New games extend this template — see `MagicSelectedCardPanel` and
// `PokemonSelectedCardPanel` for the canonical patterns.

#include "ccm/domain/Enums.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/ui/ImageViewerDialog.hpp"
#include "ccm/ui/SvgIcons.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/app.h>
#include <wx/arrstr.h>
#include <wx/bitmap.h>
#include <wx/colour.h>
#include <wx/event.h>
#include <wx/image.h>
#include <wx/listbox.h>
#include <wx/mstream.h>
#include <wx/panel.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace ccm::ui {

// Single shared event raised whenever a preview fetch resolves.
// `event.GetString()` carries the human-readable status (empty on success,
// non-empty on failure). Defined once in `BaseEvents.cpp`.
wxDECLARE_EVENT(EVT_PREVIEW_STATUS, wxCommandEvent);

template <typename TCard>
class BaseSelectedCardPanel : public wxPanel {
public:
    using card_type = TCard;

    void setCard(std::optional<TCard> card) {
        const std::optional<std::uint32_t> newId =
            card ? std::optional<std::uint32_t>{card->id} : std::nullopt;
        const bool fetchTargetChanged = (newId != lastFetchedId_);

        card_ = std::move(card);

        auto applyFlagsRow = [this](bool any) {
            flagsRow_->Layout();
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
            for (auto& row : detailRows_) {
                row.value->SetLabelText(row.emptyLabel);
            }
            applyNote("");
            for (auto& fi : flagIcons_) fi.icon->Show(false);
            applyFlagsRow(false);
            if (fetchTargetChanged) {
                state_->currentGen.fetch_add(1);
                lastFetchedId_.reset();
                clearPreview();
                previewStatus_->SetLabelText("");
                emitPreviewStatus("");
            }
        } else {
            const auto& c = *card_;
            // First row is "Name" by convention; we paint it before others so
            // it appears at the top with the literal card name.
            for (auto& row : detailRows_) {
                row.value->SetLabelText(detailValueFor(c, row.key));
            }
            applyNote(detailValueFor(c, kNoteKey));
            bool anyFlag = false;
            for (auto& fi : flagIcons_) {
                const bool on = isFlagSet(c, fi.key);
                fi.icon->Show(on);
                if (on) anyFlag = true;
            }
            applyFlagsRow(anyFlag);
            if (fetchTargetChanged) startPreviewFetch(c);
        }
        rebuildImageList();
        Layout();
    }

    void applyTheme(const ThemePalette& palette) {
        SetBackgroundColour(palette.panelBg);
        SetForegroundColour(palette.text);
        if (previewStatus_ != nullptr) {
            previewStatus_->SetBackgroundColour(palette.panelBg);
            previewStatus_->SetForegroundColour(palette.text);
        }
        for (auto& row : detailRows_) {
            if (row.label != nullptr) {
                row.label->SetBackgroundColour(palette.panelBg);
                row.label->SetForegroundColour(palette.text);
            }
            if (row.value != nullptr) {
                row.value->SetBackgroundColour(palette.panelBg);
                row.value->SetForegroundColour(palette.text);
            }
        }
        flagsRow_->SetBackgroundColour(palette.panelBg);
        flagsRow_->SetForegroundColour(palette.text);
        if (flagsLabel_ != nullptr) {
            flagsLabel_->SetBackgroundColour(palette.panelBg);
            flagsLabel_->SetForegroundColour(palette.text);
        }
        if (noteLabel_ != nullptr) {
            noteLabel_->SetBackgroundColour(palette.panelBg);
            noteLabel_->SetForegroundColour(palette.text);
        }
        if (noteValue_ != nullptr) {
            noteValue_->SetBackgroundColour(palette.panelBg);
            noteValue_->SetForegroundColour(palette.text);
        }
        imageList_->SetBackgroundColour(palette.inputBg);
        imageList_->SetForegroundColour(palette.inputText);

        const std::string textHex = palette.text.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
        for (auto& fi : flagIcons_) {
            fi.icon->SetBitmap(svgIconBitmap(fi.svg, kFlagIconSize, textHex.c_str()));
        }
        Layout();
        Refresh();
    }

    ~BaseSelectedCardPanel() override {
        // Detach any in-flight worker: late `CallAfter` lambdas check `alive`
        // before touching `panel` so they become no-ops after destruction.
        if (state_) {
            state_->alive.store(false);
            state_->panel = nullptr;
        }
    }

protected:
    // Hook descriptors --------------------------------------------------------

    // Detail row keys are integers chosen by the subclass; the base just
    // forwards them to `detailValueFor`. Reserve negatives for built-ins.
    using DetailKey = int;
    static constexpr DetailKey kNoteKey = -1;

    struct DetailRowSpec {
        std::string label;
        DetailKey   key;
        std::string emptyLabel;  // shown when `card_ == nullopt`
    };

    struct FlagIconSpec {
        const char* svg;
        const char* tooltip;
        DetailKey   key;
    };

    // Subclass declares the labelled value rows of the detail grid (excluding
    // the trailing "Note" row; that one is always present and conventionally
    // appended right before the image list).
    [[nodiscard]] virtual std::vector<DetailRowSpec> declareDetailRows() const = 0;

    // Subclass declares the flag-icon strip. Order matters — icons render
    // left-to-right in the same order as this list.
    [[nodiscard]] virtual std::vector<FlagIconSpec> declareFlagIcons() const = 0;

    // Look up the string value for a detail-row key. The base also calls this
    // with `kNoteKey` to fetch the note for the bottom row.
    [[nodiscard]] virtual std::string detailValueFor(const TCard& card, DetailKey key) const = 0;

    [[nodiscard]] virtual bool isFlagSet(const TCard& card, DetailKey key) const = 0;

    // Lookup key for the preview API: (name, setId, setNo). setNo can be
    // empty for games that don't use it (Magic).
    [[nodiscard]] virtual std::tuple<std::string, std::string, std::string>
        previewKey(const TCard& card) const = 0;

    [[nodiscard]] virtual Game gameId() const noexcept = 0;

    // Construction ------------------------------------------------------------

    BaseSelectedCardPanel(wxWindow* parent,
                          ImageService& imageService,
                          CardPreviewService& cardPreview)
        : wxPanel(parent, wxID_ANY),
          imageService_(imageService),
          cardPreview_(cardPreview),
          state_(std::make_shared<PreviewState>()) {
        state_->panel = this;
    }

    // Subclass calls this once from its constructor after the virtual hooks
    // are reachable.
    void buildLayout() {
        auto* root = new wxBoxSizer(wxVERTICAL);

        previewBitmap_ = new wxStaticBitmap(this, wxID_ANY, makePreviewPlaceholder());
        previewStatus_ = new wxStaticText(this, wxID_ANY, "");
        root->Add(previewBitmap_, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP | wxBOTTOM, 6);
        root->Add(previewStatus_, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, 4);

        buildInfoGrid(root);

        SetSizer(root);
        setCard(std::nullopt);
    }

private:
    // Shared state for the async preview fetcher.
    struct PreviewState {
        std::atomic<bool>          alive{true};
        std::atomic<unsigned>      currentGen{0};
        BaseSelectedCardPanel*     panel;
    };

    struct DetailRow {
        wxStaticText* label;
        wxStaticText* value;
        DetailKey     key;
        std::string   emptyLabel;
    };

    struct FlagIcon {
        wxStaticBitmap* icon;
        const char*     svg;
        DetailKey       key;
    };

    static constexpr int kPreviewWidth  = 250;
    static constexpr int kPreviewHeight = 350;
    static constexpr int kImageListWidth  = 0;
    static constexpr int kImageListHeight = 80;
    static constexpr int kFlagIconSize    = 14;

    static wxBitmap makePreviewPlaceholder() {
        wxImage img(kPreviewWidth, kPreviewHeight);
        img.SetAlpha();
        if (auto* alpha = img.GetAlpha()) {
            std::fill(alpha, alpha + kPreviewWidth * kPreviewHeight, 0);
        }
        return wxBitmap(img);
    }

    void buildInfoGrid(wxBoxSizer* root) {
        auto* grid = new wxFlexGridSizer(/*cols=*/2, /*vgap=*/4, /*hgap=*/12);
        grid->AddGrowableCol(1, 1);

        auto makeBoldLabel = [this](const wxString& text) {
            auto* lbl = new wxStaticText(this, wxID_ANY, text);
            wxFont lf = lbl->GetFont();
            lf.MakeBold();
            lbl->SetFont(lf);
            return lbl;
        };

        auto specs = declareDetailRows();
        detailRows_.reserve(specs.size());
        for (const auto& spec : specs) {
            auto* lbl = makeBoldLabel(spec.label);
            auto* val = new wxStaticText(this, wxID_ANY, "");
            grid->Add(lbl, 0, wxALIGN_TOP | wxALIGN_LEFT);
            grid->Add(val, 1, wxEXPAND | wxALIGN_LEFT);
            detailRows_.push_back({lbl, val, spec.key, spec.emptyLabel});
        }

        // Flags row: empty label cell, value cell holds the icon strip.
        flagsLabel_ = new wxStaticText(this, wxID_ANY, "");
        flagsRow_   = new wxPanel(this, wxID_ANY);
        auto* flagsSizer = new wxBoxSizer(wxHORIZONTAL);
        const std::string textHex =
            wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT)
                .GetAsString(wxC2S_HTML_SYNTAX)
                .ToStdString();
        auto flagSpecs = declareFlagIcons();
        flagIcons_.reserve(flagSpecs.size());
        for (const auto& fs : flagSpecs) {
            auto* ico = new wxStaticBitmap(flagsRow_, wxID_ANY,
                svgIconBitmap(fs.svg, kFlagIconSize, textHex.c_str()));
            ico->SetToolTip(fs.tooltip);
            flagsSizer->Add(ico, 0, wxRIGHT, 6);
            flagIcons_.push_back({ico, fs.svg, fs.key});
        }
        flagsRow_->SetSizer(flagsSizer);
        grid->Add(flagsLabel_, 0, wxALIGN_TOP | wxALIGN_LEFT);
        grid->Add(flagsRow_,   0, wxEXPAND);

        // Note row.
        noteLabel_ = makeBoldLabel("Note");
        noteValue_ = new wxStaticText(this, wxID_ANY, "");
        grid->Add(noteLabel_, 0, wxALIGN_TOP | wxALIGN_LEFT);
        grid->Add(noteValue_, 1, wxEXPAND | wxALIGN_LEFT);

        // Image list row.
        imageList_ = new wxListBox(this, wxID_ANY,
                                   wxDefaultPosition,
                                   wxSize(kImageListWidth, kImageListHeight),
                                   0, nullptr, wxLB_SINGLE);
        imageList_->Bind(wxEVT_LISTBOX_DCLICK, &BaseSelectedCardPanel::onImageActivated, this);
        grid->Add(makeBoldLabel("Images"), 0, wxALIGN_TOP | wxALIGN_LEFT);
        grid->Add(imageList_, 1, wxEXPAND);

        root->Add(grid, 1, wxEXPAND | wxALL, 8);
    }

    void clearPreview() {
        previewBitmap_->SetBitmap(makePreviewPlaceholder());
    }

    void startPreviewFetch(const TCard& card) {
        clearPreview();
        previewStatus_->SetLabelText("Loading preview...");
        emitPreviewStatus("");
        lastFetchedId_ = card.id;
        Layout();

        const unsigned gen = state_->currentGen.fetch_add(1) + 1;

        auto state = state_;
        CardPreviewService* svcPtr = &cardPreview_;
        auto [name, setId, setNo] = previewKey(card);
        const Game game = gameId();

        std::thread([state, gen, svcPtr, name = std::move(name),
                     setId = std::move(setId), setNo = std::move(setNo), game]() {
            auto bytes = svcPtr->fetchPreviewBytes(game, name, setId, setNo);
            const bool ok = bytes.isOk();
            std::string payload = ok ? std::move(bytes).value() : std::string{};
            std::string err     = ok ? std::string{}            : bytes.error();
            wxTheApp->CallAfter(
                [state, gen, ok, payload = std::move(payload), err = std::move(err)]() mutable {
                    if (!state->alive.load()) return;
                    if (state->currentGen.load() != gen) return;
                    if (state->panel == nullptr) return;
                    state->panel->onPreviewBytes(gen, ok, std::move(payload), std::move(err));
                });
        }).detach();
    }

    void onPreviewBytes(unsigned gen, bool ok,
                        std::string bytes, std::string err) {
        if (gen != state_->currentGen.load()) return;
        if (!ok || bytes.empty()) {
            previewStatus_->SetLabelText("(no preview available)");
            clearPreview();
            Layout();
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

    void emitPreviewStatus(const wxString& message) {
        wxCommandEvent ev(EVT_PREVIEW_STATUS, GetId());
        ev.SetEventObject(this);
        ev.SetString(message);
        if (auto* parent = GetParent()) {
            parent->GetEventHandler()->ProcessEvent(ev);
        } else {
            ProcessWindowEvent(ev);
        }
    }

    void rebuildImageList() {
        imageList_->Clear();
        if (!card_) return;
        for (std::size_t i = 0; i < card_->images.size(); ++i) {
            imageList_->Append(wxString::Format("Image %zu", i + 1));
        }
    }

    void onImageActivated(wxCommandEvent& event) {
        if (!card_) return;
        const int sel = event.GetSelection();
        if (sel < 0 || static_cast<std::size_t>(sel) >= card_->images.size()) return;

        std::vector<std::filesystem::path> paths;
        paths.reserve(card_->images.size());
        for (const auto& name : card_->images) {
            paths.push_back(imageService_.resolveImagePath(gameId(), name));
        }
        ImageViewerDialog dlg(this, std::move(paths), static_cast<std::size_t>(sel));
        const Theme theme = inferThemeFromWindow(this);
        applyThemeToWindowTree(&dlg, paletteForTheme(theme), theme);
        dlg.ShowModal();
    }

    ImageService&       imageService_;
    CardPreviewService& cardPreview_;
    std::optional<TCard> card_;

    wxStaticBitmap* previewBitmap_{nullptr};
    wxStaticText*   previewStatus_{nullptr};

    std::vector<DetailRow> detailRows_;

    wxStaticText*   flagsLabel_{nullptr};
    wxPanel*        flagsRow_{nullptr};
    std::vector<FlagIcon> flagIcons_;

    wxStaticText* noteLabel_{nullptr};
    wxStaticText* noteValue_{nullptr};

    wxListBox* imageList_{nullptr};

    std::shared_ptr<PreviewState> state_;
    std::optional<std::uint32_t>  lastFetchedId_;
};

}  // namespace ccm::ui
