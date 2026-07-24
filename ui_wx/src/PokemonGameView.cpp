#include "ccm/ui/PokemonGameView.hpp"

#include "ccm/games/pokemon/PokemonCollectionSetSync.hpp"
#include "ccm/games/pokemon/PokemonSetSource.hpp"
#include "ccm/games/pokemonjp/JapanesePokemonSetSource.hpp"
#include "ccm/ui/CardEditModalGuard.hpp"
#include "ccm/ui/PokemonCardEditDialog.hpp"
#include "ccm/ui/PokemonCardListPanel.hpp"
#include "ccm/ui/PokemonSelectedCardPanel.hpp"
#include "ccm/ui/PokemonSetCompletionPanel.hpp"
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
constexpr int kPokeToolbarIconPx = 18;
constexpr const char kPokeFilterHint[] = "Filter";

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

PokemonGameView::PokemonGameView(ConfigService&                         config,
                                 CollectionService<PokemonCard>&        collection,
                                 SetService&                            sets,
                                 ImageService&                          images,
                                 CardPreviewService&                    cardPreview,
                                 IGameModule&                           westModule,
                                 IGameModule&                           asiaModule,
                                 PokemonSetCatalogService&              catalogStore)
    : config_(config),
      collection_(collection),
      sets_(sets),
      images_(images),
      cardPreview_(cardPreview),
      westModule_(westModule),
      asiaModule_(asiaModule),
      catalogStore_(catalogStore) {}

void PokemonGameView::ensureSetsLoaded() {
    if (attemptedInitialSetLoad_) return;
    attemptedInitialSetLoad_ = true;

    auto loadOrRefresh = [this](Game game, std::vector<Set>& cache) {
        auto cached = sets_.getSets(game);
        if (cached) {
            cache = std::move(cached).value();
            if (!cache.empty()) return;
        } else {
            cache.clear();
        }
        auto refreshed = sets_.updateSets(game);
        if (refreshed) {
            cache = std::move(refreshed).value();
        }
    };

    loadOrRefresh(Game::Pokemon, setsCacheWest_);
    loadOrRefresh(Game::JapanesePokemon, setsCacheAsia_);
}

void PokemonGameView::ensureSingleCardsMounted(wxWindow* splitterParent) {
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

void PokemonGameView::buildSingleCardsToolbar(wxWindow* parent, wxBoxSizer* pageSizer) {
    auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
    auto makeToolBtn = [&](const char* svg, const wxString& tip) {
        wxBitmap bmp = svgIconBitmap(svg, kPokeToolbarIconPx, "#000000");
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
    filterInput_->SetHint(kPokeFilterHint);
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

void PokemonGameView::refreshToolbarIcons(const ThemePalette& palette) {
    const std::string tbHex = palette.buttonText.GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
    if (toolbarButtons_[0]) {
        toolbarButtons_[0]->SetBitmap(
            svgIconBitmap(kSvgToolbarAdd, kPokeToolbarIconPx, tbHex.c_str()));
    }
    if (toolbarButtons_[1]) {
        toolbarButtons_[1]->SetBitmap(
            svgIconBitmap(kSvgToolbarEdit, kPokeToolbarIconPx, tbHex.c_str()));
    }
    if (toolbarButtons_[2]) {
        toolbarButtons_[2]->SetBitmap(
            svgIconBitmap(kSvgToolbarDelete, kPokeToolbarIconPx, tbHex.c_str()));
    }
}

void PokemonGameView::selectTab(int index) {
    if (index < 0 || index > 1 || book_ == nullptr) return;
    activeTab_ = index;
    book_->SetSelection(index);
    refreshTabBarTheme(paletteForTheme(config_.current().theme));
}

void PokemonGameView::refreshTabBarTheme(const ThemePalette& palette) {
    if (tabBar_ == nullptr) return;

    const wxColour barBg = palette.panelBg;
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

void PokemonGameView::buildTabBar(wxWindow* parent, wxBoxSizer* rootSizer) {
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

wxPanel* PokemonGameView::contentPanel(wxWindow* parent) {
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

        setCompletionPanel_ = new PokemonSetCompletionPanel(book_, catalogStore_);
        setCompletionPanel_->reloadFromStore();
        book_->AddPage(setCompletionPanel_, "Set Completion");

        root->Add(book_, 1, wxEXPAND | wxTOP, 5);
        contentPanel_->SetSizer(root);

        selectTab(0);
        refreshToolbarIcons(paletteForTheme(config_.current().theme));
        contentPanel_->CallAfter([this]() {
            refreshTabBarTheme(paletteForTheme(config_.current().theme));
        });
    }
    return contentPanel_;
}

wxPanel* PokemonGameView::listPanel(wxWindow* parent) {
    if (listPanel_ == nullptr) {
        listPanel_ = new PokemonCardListPanel(parent);
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

wxPanel* PokemonGameView::selectedPanel(wxWindow* parent) {
    if (selectedPanel_ == nullptr) {
        selectedPanel_ = new PokemonSelectedCardPanel(parent, images_, cardPreview_);
    }
    return selectedPanel_;
}

void PokemonGameView::refreshCollection(std::optional<std::uint32_t> selectId) {
    if (contentPanel_ == nullptr && listPanel_ == nullptr) return;

    auto loaded = collection_.list(Game::Pokemon);
    if (!loaded) {
        showThemedMessageDialog(nullptr, "Failed to load Pokemon collection: " + loaded.error(),
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

const std::vector<Set>& PokemonGameView::setsForDialog(PokemonRegion region) {
    ensureSetsLoaded();
    if (region == PokemonRegion::Asia) {
        if (!setsCacheAsia_.empty()) return setsCacheAsia_;
        auto loaded = sets_.getSets(Game::JapanesePokemon);
        if (loaded) setsCacheAsia_ = std::move(loaded).value();
        else        setsCacheAsia_.clear();
        return setsCacheAsia_;
    }
    if (!setsCacheWest_.empty()) return setsCacheWest_;
    auto loaded = sets_.getSets(Game::Pokemon);
    if (loaded) setsCacheWest_ = std::move(loaded).value();
    else        setsCacheWest_.clear();
    return setsCacheWest_;
}

void PokemonGameView::onAddCard(wxWindow* parentWindow) {
    if (cardEditModalIsActive()) {
        showThemedMessageDialog(parentWindow, wxString::FromUTF8(kCardEditModalBlockedUtf8),
                                wxString::FromUTF8("Add card"), wxOK | wxICON_INFORMATION);
        return;
    }
    PokemonCard fresh;
    fresh.amount = 1;
    fresh.region = PokemonRegion::West;
    fresh.language = Language::English;
    fresh.condition = Condition::NearMint;

    PokemonCardEditDialog dlg(parentWindow, images_, sets_, cardPreview_, EditMode::Create, fresh,
                              &setsForDialog(PokemonRegion::West),
                              &setsForDialog(PokemonRegion::Asia));
    themeModalDialog(&dlg, config_.current().theme);
    CardEditModalGuard modalGuard;
    if (dlg.ShowModal() != wxID_OK) return;

    auto added = collection_.add(Game::Pokemon, dlg.card());
    if (!added) {
        showThemedMessageDialog(parentWindow, "Failed to add card: " + added.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }

    PokemonCard persisted = dlg.card();
    persisted.id = added.value();
    auto normalized = images_.normalizeNamesForPersistedCard(
        Game::Pokemon, persisted.id, persisted.set.name, persisted.name, persisted.images);
    if (normalized) {
        if (normalized.value() != persisted.images) {
            persisted.images = std::move(normalized).value();
            auto updated = collection_.update(Game::Pokemon, persisted);
            if (!updated) {
                showThemedMessageDialog(parentWindow, "Card added, but image name normalization failed to persist: " + updated.error(),
                                        "Warning", wxOK | wxICON_WARNING);
            }
        }
    } else {
        showThemedMessageDialog(parentWindow, "Card added, but image rename to ID-prefixed format failed: " + normalized.error(),
                                "Warning", wxOK | wxICON_WARNING);
    }
    refreshCollection(added.value());
}

void PokemonGameView::onEditCard(wxWindow* parentWindow) {
    if (listPanel_ == nullptr) return;
    auto sel = listPanel_->selected();
    if (!sel) {
        showThemedMessageDialog(parentWindow, "Select a card first.", "Edit", wxOK | wxICON_INFORMATION);
        return;
    }
    if (cardEditModalIsActive()) {
        showThemedMessageDialog(parentWindow, wxString::FromUTF8(kCardEditModalBlockedUtf8),
                                wxString::FromUTF8("Edit"), wxOK | wxICON_INFORMATION);
        return;
    }
    PokemonCardEditDialog dlg(parentWindow, images_, sets_, cardPreview_, EditMode::Edit, *sel,
                              &setsForDialog(PokemonRegion::West),
                              &setsForDialog(PokemonRegion::Asia));
    themeModalDialog(&dlg, config_.current().theme);
    CardEditModalGuard modalGuard;
    if (dlg.ShowModal() != wxID_OK) return;
    auto updated = collection_.update(Game::Pokemon, dlg.card());
    if (!updated) {
        showThemedMessageDialog(parentWindow, "Failed to update card: " + updated.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    refreshCollection();
}

void PokemonGameView::onDeleteCard(wxWindow* parentWindow) {
    if (listPanel_ == nullptr) return;
    auto sel = listPanel_->selected();
    if (!sel) {
        showThemedMessageDialog(parentWindow, "Select a card first.", "Delete", wxOK | wxICON_INFORMATION);
        return;
    }
    if (showThemedConfirmDialog(parentWindow, "Delete \"" + sel->name + "\"?",
                                "Confirm") != wxID_YES) {
        return;
    }
    auto removed = collection_.remove(Game::Pokemon, sel->id);
    if (!removed) {
        showThemedMessageDialog(parentWindow, "Failed to delete card: " + removed.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    refreshCollection();
}

std::string PokemonGameView::onUpdateSets(wxWindow* parentWindow) {
    auto* westSrc = dynamic_cast<PokemonSetSource*>(&westModule_.setSource());
    auto* asiaSrc = dynamic_cast<JapanesePokemonSetSource*>(&asiaModule_.setSource());
    if (westSrc == nullptr || asiaSrc == nullptr) {
        showThemedMessageDialog(parentWindow, "Pokemon set source unavailable.",
                                "Error", wxOK | wxICON_ERROR);
        return "Update failed";
    }

    auto westBoth = westSrc->fetchAllWithCatalog();
    auto asiaBoth = asiaSrc->fetchAllWithCatalog();

    std::string westErr;
    std::string asiaErr;
    std::size_t westSets = 0;
    std::size_t asiaSets = 0;
    std::size_t westPacks = 0;
    std::size_t asiaPacks = 0;

    if (westBoth) {
        auto savedSets = sets_.saveSets(Game::Pokemon, westBoth.value().sets);
        if (!savedSets) {
            westErr = savedSets.error();
        } else {
            auto savedCatalog =
                catalogStore_.save(PokemonRegion::West, westBoth.value().catalog);
            if (!savedCatalog) {
                westErr = "sets saved, but catalog failed: " + savedCatalog.error();
            }
            setsCacheWest_ = westBoth.value().sets;
            westSets = westBoth.value().sets.size();
            westPacks = westBoth.value().catalog.packs.size();
        }
    } else {
        westErr = westBoth.error();
    }

    if (asiaBoth) {
        auto savedSets = sets_.saveSets(Game::JapanesePokemon, asiaBoth.value().sets);
        if (!savedSets) {
            asiaErr = savedSets.error();
        } else {
            auto savedCatalog =
                catalogStore_.save(PokemonRegion::Asia, asiaBoth.value().catalog);
            if (!savedCatalog) {
                asiaErr = "sets saved, but catalog failed: " + savedCatalog.error();
            }
            setsCacheAsia_ = asiaBoth.value().sets;
            asiaSets = asiaBoth.value().sets.size();
            asiaPacks = asiaBoth.value().catalog.packs.size();
        }
    } else {
        asiaErr = asiaBoth.error();
    }

    std::size_t collectionSynced = 0;
    std::string collectionErr;
    // Sync collection against whichever set lists we successfully refreshed
    // (and any still-cached lists from a prior Update).
    {
        auto loaded = collection_.list(Game::Pokemon);
        if (!loaded) {
            collectionErr = loaded.error();
        } else {
            auto cards = std::move(loaded).value();
            collectionSynced =
                syncPokemonCollectionSets(cards, setsCacheWest_, setsCacheAsia_);
            if (collectionSynced > 0) {
                auto saved = collection_.saveAll(Game::Pokemon, std::move(cards));
                if (!saved) {
                    collectionErr = saved.error();
                    collectionSynced = 0;
                } else {
                    refreshCollection();
                }
            }
        }
    }

    if (setCompletionPanel_ != nullptr) {
        setCompletionPanel_->reloadFromStore();
        if (auto loaded = collection_.list(Game::Pokemon)) {
            setCompletionPanel_->setCollection(std::move(loaded).value());
        }
    }

    if (!westErr.empty() && !asiaErr.empty()) {
        showThemedMessageDialog(
            parentWindow,
            "Failed to update West: " + westErr + "\nFailed to update Asia: " + asiaErr,
            "Error", wxOK | wxICON_ERROR);
        return "Update failed";
    }
    if (!westErr.empty()) {
        showThemedMessageDialog(
            parentWindow,
            "Updated " + std::to_string(asiaSets) + " Asia sets / " +
                std::to_string(asiaPacks) + " checklists, but West failed: " + westErr,
            "Sets partially updated", wxOK | wxICON_WARNING);
        return "Pokemon sets partially updated.";
    }
    if (!asiaErr.empty()) {
        showThemedMessageDialog(
            parentWindow,
            "Updated " + std::to_string(westSets) + " West sets / " +
                std::to_string(westPacks) + " checklists, but Asia failed: " + asiaErr,
            "Sets partially updated", wxOK | wxICON_WARNING);
        return "Pokemon sets partially updated.";
    }

    std::string body = "Updated " + std::to_string(westSets) + " West sets (" +
                       std::to_string(westPacks) + " checklists) and " +
                       std::to_string(asiaSets) + " Asia sets (" +
                       std::to_string(asiaPacks) + " checklists).";
    if (collectionSynced > 0) {
        body += "\nSynced set metadata on " + std::to_string(collectionSynced) +
                " collection card(s).";
    }
    if (!collectionErr.empty()) {
        body += "\nCollection sync failed: " + collectionErr;
        showThemedMessageDialog(parentWindow, body, "Sets updated",
                                wxOK | wxICON_WARNING);
        return "Pokemon sets updated.";
    }
    showThemedMessageDialog(parentWindow, body, "Sets updated",
                            wxOK | wxICON_INFORMATION);
    return "Pokemon sets updated.";
}

void PokemonGameView::setFilter(std::string_view filter) {
    if (filterInput_ != nullptr) {
        const wxString wanted = wxString::FromUTF8(std::string(filter).c_str());
        if (filterInput_->GetValue() != wanted) {
            filterInput_->ChangeValue(wanted);
            if (filter.empty()) {
                filterInput_->SetHint(kPokeFilterHint);
                filterInput_->Refresh();
            }
        }
    }
    if (listPanel_) listPanel_->setFilter(filter);
}

void PokemonGameView::nudgeSelection(int delta) {
    if (listPanel_) listPanel_->nudgeSelection(delta);
}

void PokemonGameView::applyTheme(const ThemePalette& palette) {
    if (contentPanel_) applyThemeToWindowTree(contentPanel_, palette, config_.current().theme);
    if (listPanel_)     listPanel_->applyTheme(palette);
    if (selectedPanel_) selectedPanel_->applyTheme(palette);
    if (setCompletionPanel_) setCompletionPanel_->applyTheme(palette);
    refreshToolbarIcons(palette);
    refreshTabBarTheme(palette);
    if (filterInput_ != nullptr) {
        filterInput_->SetBackgroundColour(palette.inputBg);
        filterInput_->SetForegroundColour(palette.inputText);
        filterInput_->SetOwnBackgroundColour(palette.inputBg);
        filterInput_->SetOwnForegroundColour(palette.inputText);
    }
}

}  // namespace ccm::ui
