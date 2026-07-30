#include "ccm/ui/DigiBattle99GameView.hpp"

#include "ccm/games/digibattle99/DigiBattle99SetSource.hpp"
#include "ccm/ui/CardEditModalGuard.hpp"
#include "ccm/ui/DigiBattle99CardEditDialog.hpp"
#include "ccm/ui/DigiBattle99CardListPanel.hpp"
#include "ccm/ui/DigiBattle99SelectedCardPanel.hpp"
#include "ccm/ui/DigiBattle99SetCompletionPanel.hpp"
#include "ccm/ui/SvgIcons.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/bmpbuttn.h>
#include <wx/dcclient.h>
#include <wx/panel.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/window.h>

#include <string>

namespace ccm::ui {

namespace {
constexpr int kDigiToolbarIconPx = 18;
constexpr const char kDigiFilterHint[] = "Filter";

wxColour lighten(const wxColour& c, int amount) {
    auto lift = [amount](unsigned char channel) -> unsigned char {
        const int raised = static_cast<int>(channel) + amount;
        return static_cast<unsigned char>(raised > 255 ? 255 : raised);
    };
    return wxColour(lift(c.Red()), lift(c.Green()), lift(c.Blue()));
}

wxColour darken(const wxColour& c, int amount) {
    auto drop = [amount](unsigned char channel) -> unsigned char {
        const int lowered = static_cast<int>(channel) - amount;
        return static_cast<unsigned char>(lowered < 0 ? 0 : lowered);
    };
    return wxColour(drop(c.Red()), drop(c.Green()), drop(c.Blue()));
}
}  // namespace

DigiBattle99GameView::DigiBattle99GameView(ConfigService&                         config,
                                           CollectionService<DigiBattle99Card>&   collection,
                                           SetService&                            sets,
                                           ImageService&                          images,
                                           CardPreviewService&                    cardPreview,
                                           IGameModule&                           module,
                                           DigiBattle99SetCatalogService&         catalogStore)
    : config_(config),
      collection_(collection),
      sets_(sets),
      images_(images),
      cardPreview_(cardPreview),
      module_(module),
      catalogStore_(catalogStore) {}

void DigiBattle99GameView::ensureSetsLoaded() {
    if (attemptedInitialSetLoad_) return;
    attemptedInitialSetLoad_ = true;

    auto cached = sets_.getSets(Game::DigiBattle99);
    if (cached) {
        setsCache_ = std::move(cached).value();
        if (!setsCache_.empty()) return;
    } else {
        setsCache_.clear();
    }

    auto refreshed = sets_.updateSets(Game::DigiBattle99);
    if (refreshed) {
        setsCache_ = std::move(refreshed).value();
    }
}

void DigiBattle99GameView::ensureSingleCardsMounted(wxWindow* splitterParent) {
    if (singleSplitter_ == nullptr) {
        singleSplitter_ = new wxSplitterWindow(splitterParent, wxID_ANY, wxDefaultPosition,
                                               wxDefaultSize, wxSP_LIVE_UPDATE);
        singleSplitter_->SetMinimumPaneSize(280);
    }
    auto* list = listPanel(singleSplitter_);
    auto* selected = selectedPanel(singleSplitter_);
    if (!singleSplitter_->IsSplit()) {
        singleSplitter_->SplitVertically(selected, list, 360);
    }
}

void DigiBattle99GameView::buildSingleCardsToolbar(wxWindow* parent, wxBoxSizer* pageSizer) {
    auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
    auto makeToolBtn = [&](const char* svg, const wxString& tip) {
        wxBitmap bmp = svgIconBitmap(svg, kDigiToolbarIconPx, "#000000");
        auto* b = new wxBitmapButton(parent, wxID_ANY, bmp, wxDefaultPosition, wxDefaultSize,
                                     wxBU_EXACTFIT);
        b->SetToolTip(tip);
        return b;
    };
    toolbarButtons_[0] = makeToolBtn(kSvgToolbarAdd,    "Add Card");
    toolbarButtons_[1] = makeToolBtn(kSvgToolbarEdit,   "Edit");
    toolbarButtons_[2] = makeToolBtn(kSvgToolbarDelete, "Delete");
    toolbar->AddSpacer(4);
    toolbar->Add(toolbarButtons_[0], 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    toolbar->Add(toolbarButtons_[1], 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    toolbar->Add(toolbarButtons_[2], 0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    toolbar->AddStretchSpacer(1);
    filterInput_ = new wxTextCtrl(parent, wxID_ANY, "", wxDefaultPosition, wxSize(260, -1));
    filterInput_->SetHint(kDigiFilterHint);
    toolbar->Add(filterInput_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxTOP | wxBOTTOM, 4);
    pageSizer->Add(toolbar, 0, wxEXPAND);

    toolbarButtons_[0]->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxWindow* owner = wxGetTopLevelParent(contentPanel_);
        onAddCard(owner != nullptr ? owner : contentPanel_);
    });
    toolbarButtons_[1]->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxWindow* owner = wxGetTopLevelParent(contentPanel_);
        onEditCard(owner != nullptr ? owner : contentPanel_);
    });
    toolbarButtons_[2]->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        wxWindow* owner = wxGetTopLevelParent(contentPanel_);
        onDeleteCard(owner != nullptr ? owner : contentPanel_);
    });
    filterInput_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
        if (filterInput_ == nullptr) return;
        setFilter(filterInput_->GetValue().ToStdString(wxConvUTF8));
    });
    filterInput_->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& ev) {
        const int code = ev.GetKeyCode();
        if (code == WXK_UP || code == WXK_DOWN) {
            nudgeSelection(code == WXK_UP ? -1 : 1);
            return;
        }
        ev.Skip();
    });
}

void DigiBattle99GameView::refreshToolbarIcons(const ThemePalette& palette) {
    const std::string tbHex = palette.buttonText.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
    if (toolbarButtons_[0]) {
        toolbarButtons_[0]->SetBitmap(
            svgIconBitmap(kSvgToolbarAdd, kDigiToolbarIconPx, tbHex.c_str()));
    }
    if (toolbarButtons_[1]) {
        toolbarButtons_[1]->SetBitmap(
            svgIconBitmap(kSvgToolbarEdit, kDigiToolbarIconPx, tbHex.c_str()));
    }
    if (toolbarButtons_[2]) {
        toolbarButtons_[2]->SetBitmap(
            svgIconBitmap(kSvgToolbarDelete, kDigiToolbarIconPx, tbHex.c_str()));
    }
}

void DigiBattle99GameView::selectTab(int index) {
    if (index < 0 || index > 1 || book_ == nullptr) return;
    activeTab_ = index;
    book_->SetSelection(index);
    refreshTabBarTheme(paletteForTheme(config_.current().theme));
}

void DigiBattle99GameView::refreshTabBarTheme(const ThemePalette& palette) {
    if (tabBar_ == nullptr) return;

    const wxColour barBg = palette.panelBg;
    // Match toolbar button plate (Add/Edit/Delete), not a darker inset fill.
    const wxColour tabBg = palette.buttonBg;

    tabBar_->SetBackgroundColour(barBg);
    tabBar_->SetOwnBackgroundColour(barBg);

    for (int i = 0; i < 2; ++i) {
        auto* tab = tabPanels_[i];
        auto* label = tabLabels_[i];
        if (tab == nullptr || label == nullptr) continue;
        const bool selected = (i == activeTab_);
        tab->SetBackgroundColour(tabBg);
        tab->SetOwnBackgroundColour(tabBg);
        // Keep the label plate identical to the tab fill so a late theme pass
        // cannot leave a darker box around the caption.
        label->SetBackgroundColour(tabBg);
        label->SetOwnBackgroundColour(tabBg);
        label->SetForegroundColour(palette.text);
        label->SetOwnForegroundColour(palette.text);
        wxFont font = label->GetFont();
        font.SetWeight(selected ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
        label->SetFont(font);
        tab->Refresh();
        label->Refresh();
    }
    tabBar_->Layout();
    tabBar_->Refresh();
}

void DigiBattle99GameView::buildTabBar(wxWindow* parent, wxBoxSizer* rootSizer) {
    tabBar_ = new wxPanel(parent, wxID_ANY);
    tabBar_->SetBackgroundStyle(wxBG_STYLE_PAINT);
    auto* tabSizer = new wxBoxSizer(wxHORIZONTAL);
    tabSizer->AddSpacer(4);

    const char* labels[2] = {"Single Cards", "Set Completion"};
    for (int i = 0; i < 2; ++i) {
        auto* tab = new wxPanel(tabBar_, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
        tab->SetCursor(wxCursor(wxCURSOR_HAND));
        tab->SetBackgroundStyle(wxBG_STYLE_PAINT);
        auto* label = new wxStaticText(tab, wxID_ANY, wxString::FromUTF8(labels[i]));
        auto* inner = new wxBoxSizer(wxVERTICAL);
        // Compact padding so the strip stays short; frame is drawn in paint.
        inner->Add(label, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 5);
        tab->SetSizer(inner);

        auto onClick = [this, i](wxMouseEvent&) { selectTab(i); };
        tab->Bind(wxEVT_LEFT_DOWN, onClick);
        label->Bind(wxEVT_LEFT_DOWN, onClick);
        tab->Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});
        tab->Bind(wxEVT_PAINT, [this, tab, i](wxPaintEvent&) {
            wxPaintDC dc(tab);
            const ThemePalette palette = paletteForTheme(config_.current().theme);
            const bool dark = config_.current().theme == Theme::Dark;
            const bool selected = (i == activeTab_);
            // Same plate as toolbar bitmap buttons.
            const wxColour bg = palette.buttonBg;
            const wxColour frame =
                dark ? lighten(palette.panelBg, 55) : darken(palette.panelBg, 45);
            const wxColour frameSel = dark ? lighten(palette.panelBg, 85) : darken(palette.panelBg, 70);
            const wxRect r = tab->GetClientRect();
            dc.SetPen(wxPen(selected ? frameSel : frame, 1));
            dc.SetBrush(wxBrush(bg));
            dc.DrawRectangle(r.x, r.y, r.width, r.height);
            if (selected) {
                dc.SetPen(wxPen(palette.text, 2));
                dc.DrawLine(r.GetLeft() + 4, r.GetBottom() - 1, r.GetRight() - 4,
                            r.GetBottom() - 1);
            }
        });

        tabPanels_[i] = tab;
        tabLabels_[i] = label;
        if (i > 0) tabSizer->AddSpacer(4);
        tabSizer->Add(tab, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 3);
    }
    tabSizer->AddStretchSpacer(1);

    tabBar_->Bind(wxEVT_PAINT, [this](wxPaintEvent&) {
        wxPaintDC dc(tabBar_);
        const ThemePalette palette = paletteForTheme(config_.current().theme);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(palette.panelBg));
        dc.DrawRectangle(tabBar_->GetClientRect());
        dc.SetPen(wxPen(darken(palette.text, 120), 1));
        const wxRect r = tabBar_->GetClientRect();
        dc.DrawLine(r.GetLeft(), r.GetBottom(), r.GetRight(), r.GetBottom());
    });
    tabBar_->Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent&) {});

    tabBar_->SetSizer(tabSizer);
    rootSizer->Add(tabBar_, 0, wxEXPAND);
    refreshTabBarTheme(paletteForTheme(config_.current().theme));
}

wxPanel* DigiBattle99GameView::contentPanel(wxWindow* parent) {
    if (contentPanel_ == nullptr) {
        contentPanel_ = new wxPanel(parent);
        auto* root = new wxBoxSizer(wxVERTICAL);

        buildTabBar(contentPanel_, root);

        book_ = new wxSimplebook(contentPanel_, wxID_ANY);
        auto* singlePage = new wxPanel(book_);
        auto* singleSizer = new wxBoxSizer(wxVERTICAL);
        buildSingleCardsToolbar(singlePage, singleSizer);
        ensureSingleCardsMounted(singlePage);
        singleSizer->Add(singleSplitter_, 1, wxEXPAND);
        singlePage->SetSizer(singleSizer);
        book_->AddPage(singlePage, "Single Cards");

        setCompletionPanel_ = new DigiBattle99SetCompletionPanel(book_, catalogStore_);
        setCompletionPanel_->reloadFromStore();
        book_->AddPage(setCompletionPanel_, "Set Completion");

        root->Add(book_, 1, wxEXPAND | wxTOP, 5);
        contentPanel_->SetSizer(root);

        selectTab(0);
        refreshToolbarIcons(paletteForTheme(config_.current().theme));
        // First mount: re-assert tab plate colors after the initial layout paint.
        contentPanel_->CallAfter([this]() {
            refreshTabBarTheme(paletteForTheme(config_.current().theme));
        });
    }
    return contentPanel_;
}

wxPanel* DigiBattle99GameView::listPanel(wxWindow* parent) {
    if (listPanel_ == nullptr) {
        listPanel_ = new DigiBattle99CardListPanel(parent);
        listPanel_->Bind(EVT_CARD_SELECTED, [this](wxCommandEvent&) {
            if (selectedPanel_ != nullptr && listPanel_ != nullptr) {
                selectedPanel_->setCard(listPanel_->selected());
            }
        });
        listPanel_->Bind(EVT_CARD_ACTIVATED, [this](wxCommandEvent&) {
            wxWindow* owner = wxGetTopLevelParent(listPanel_);
            onEditCard(owner != nullptr ? owner : static_cast<wxWindow*>(listPanel_));
        });
    }
    return listPanel_;
}

wxPanel* DigiBattle99GameView::selectedPanel(wxWindow* parent) {
    if (selectedPanel_ == nullptr) {
        selectedPanel_ = new DigiBattle99SelectedCardPanel(parent, images_, cardPreview_);
    }
    return selectedPanel_;
}

void DigiBattle99GameView::refreshCollection(std::optional<std::uint32_t> selectId) {
    // Ensure the Digimon host (and list panel) exist even when MainFrame mounts
    // via contentPanel before an explicit listPanel call.
    if (contentPanel_ == nullptr && listPanel_ == nullptr) return;

    auto loaded = collection_.list(Game::DigiBattle99);
    if (!loaded) {
        showThemedMessageDialog(
            nullptr,
            "Failed to load Digimon (Digi-Battle) collection: " + loaded.error(),
            "Error", wxOK | wxICON_ERROR);
        return;
    }
    auto cards = std::move(loaded).value();
    if (listPanel_ != nullptr) {
        listPanel_->setCards(cards, selectId);
        listPanel_->activateSelection();
        if (selectedPanel_) selectedPanel_->setCard(listPanel_->selected());
    }
    if (setCompletionPanel_ != nullptr) {
        setCompletionPanel_->setCollection(std::move(cards));
    }
}

const std::vector<Set>& DigiBattle99GameView::setsForDialog() {
    ensureSetsLoaded();
    if (!setsCache_.empty()) return setsCache_;
    auto loaded = sets_.getSets(Game::DigiBattle99);
    if (loaded) setsCache_ = std::move(loaded).value();
    else        setsCache_.clear();
    return setsCache_;
}

void DigiBattle99GameView::onAddCard(wxWindow* parentWindow) {
    if (cardEditModalIsActive()) {
        showThemedMessageDialog(parentWindow, wxString::FromUTF8(kCardEditModalBlockedUtf8),
                                wxString::FromUTF8("Add card"), wxOK | wxICON_INFORMATION);
        return;
    }
    DigiBattle99Card fresh;
    fresh.amount = 1;
    fresh.language = Language::English;
    fresh.condition = Condition::NearMint;

    DigiBattle99CardEditDialog dlg(parentWindow, images_, sets_, cardPreview_, EditMode::Create,
                                   fresh, &setsForDialog());
    themeModalDialog(&dlg, config_.current().theme);
    CardEditModalGuard modalGuard;
    if (dlg.ShowModal() != wxID_OK) return;

    auto added = collection_.add(Game::DigiBattle99, dlg.card());
    if (!added) {
        showThemedMessageDialog(parentWindow, "Failed to add card: " + added.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }

    DigiBattle99Card persisted = dlg.card();
    persisted.id = added.value();
    auto normalized = images_.normalizeNamesForPersistedCard(
        Game::DigiBattle99, persisted.id, persisted.set.name, persisted.name, persisted.images);
    if (normalized) {
        if (normalized.value() != persisted.images) {
            persisted.images = std::move(normalized).value();
            auto updated = collection_.update(Game::DigiBattle99, persisted);
            if (!updated) {
                showThemedMessageDialog(
                    parentWindow,
                    "Card added, but image name normalization failed to persist: " +
                        updated.error(),
                    "Warning", wxOK | wxICON_WARNING);
            }
        }
    } else {
        showThemedMessageDialog(
            parentWindow,
            "Card added, but image rename to ID-prefixed format failed: " + normalized.error(),
            "Warning", wxOK | wxICON_WARNING);
    }
    refreshCollection(added.value());
}

void DigiBattle99GameView::onEditCard(wxWindow* parentWindow) {
    if (listPanel_ == nullptr) return;
    auto sel = listPanel_->selected();
    if (!sel) {
        showThemedMessageDialog(parentWindow, "Select a card first.", "Edit",
                                wxOK | wxICON_INFORMATION);
        return;
    }
    if (cardEditModalIsActive()) {
        showThemedMessageDialog(parentWindow, wxString::FromUTF8(kCardEditModalBlockedUtf8),
                                wxString::FromUTF8("Edit"), wxOK | wxICON_INFORMATION);
        return;
    }
    DigiBattle99CardEditDialog dlg(parentWindow, images_, sets_, cardPreview_, EditMode::Edit,
                                   *sel, &setsForDialog());
    themeModalDialog(&dlg, config_.current().theme);
    CardEditModalGuard modalGuard;
    if (dlg.ShowModal() != wxID_OK) return;
    auto updated = collection_.update(Game::DigiBattle99, dlg.card());
    if (!updated) {
        showThemedMessageDialog(parentWindow, "Failed to update card: " + updated.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    refreshCollection();
}

void DigiBattle99GameView::onDeleteCard(wxWindow* parentWindow) {
    if (listPanel_ == nullptr) return;
    auto sel = listPanel_->selected();
    if (!sel) {
        showThemedMessageDialog(parentWindow, "Select a card first.", "Delete",
                                wxOK | wxICON_INFORMATION);
        return;
    }
    if (showThemedConfirmDialog(parentWindow, "Delete \"" + sel->name + "\"?",
                                "Confirm") != wxID_YES) {
        return;
    }
    auto removed = collection_.remove(Game::DigiBattle99, sel->id);
    if (!removed) {
        showThemedMessageDialog(parentWindow, "Failed to delete card: " + removed.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    refreshCollection();
}

std::string DigiBattle99GameView::onUpdateSets(wxWindow* parentWindow) {
    auto* digiSrc = dynamic_cast<DigiBattle99SetSource*>(&module_.setSource());
    if (digiSrc == nullptr) {
        showThemedMessageDialog(parentWindow, "Digimon Digi-Battle set source unavailable.",
                                "Error", wxOK | wxICON_ERROR);
        return "Update failed";
    }

    auto both = digiSrc->fetchAllWithCatalog();
    if (!both) {
        showThemedMessageDialog(parentWindow, "Failed to update sets: " + both.error(),
                                "Error", wxOK | wxICON_ERROR);
        return "Update failed";
    }

    auto savedSets = sets_.saveSets(Game::DigiBattle99, both.value().sets);
    if (!savedSets) {
        showThemedMessageDialog(parentWindow, "Failed to save sets: " + savedSets.error(),
                                "Error", wxOK | wxICON_ERROR);
        return "Update failed";
    }

    auto savedCatalog = catalogStore_.save(both.value().catalog);
    if (!savedCatalog) {
        showThemedMessageDialog(parentWindow,
                                "Sets saved, but set catalog failed: " + savedCatalog.error(),
                                "Warning", wxOK | wxICON_WARNING);
    }

    setsCache_ = both.value().sets;
    if (setCompletionPanel_ != nullptr) {
        setCompletionPanel_->reloadFromStore();
        if (auto loaded = collection_.list(Game::DigiBattle99)) {
            setCompletionPanel_->setCollection(std::move(loaded).value());
        }
    }

    const std::size_t setCount = both.value().sets.size();
    const std::size_t packCount = both.value().catalog.packs.size();
    showThemedMessageDialog(
        parentWindow,
        "Updated " + std::to_string(setCount) + " Digimon (Digi-Battle) sets and " +
            std::to_string(packCount) + " set checklists.",
        "Sets updated", wxOK | wxICON_INFORMATION);
    return "Digimon (Digi-Battle) sets updated.";
}

void DigiBattle99GameView::setFilter(std::string_view filter) {
    if (filterInput_ != nullptr) {
        const wxString wanted = wxString::FromUTF8(std::string(filter).c_str());
        if (filterInput_->GetValue() != wanted) {
            filterInput_->ChangeValue(wanted);
            if (filter.empty()) {
                filterInput_->SetHint(kDigiFilterHint);
                filterInput_->Refresh();
            }
        }
    }
    if (listPanel_) listPanel_->setFilter(filter);
}

void DigiBattle99GameView::nudgeSelection(int delta) {
    if (listPanel_) listPanel_->nudgeSelection(delta);
}

void DigiBattle99GameView::applyTheme(const ThemePalette& palette) {
    if (contentPanel_) applyThemeToWindowTree(contentPanel_, palette, config_.current().theme);
    if (listPanel_)     listPanel_->applyTheme(palette);
    if (selectedPanel_) selectedPanel_->applyTheme(palette);
    if (setCompletionPanel_) setCompletionPanel_->applyTheme(palette);
    refreshToolbarIcons(palette);
    refreshTabBarTheme(palette);
    applyPaletteToTextCtrl(filterInput_, palette, config_.current().theme);
}

}  // namespace ccm::ui
