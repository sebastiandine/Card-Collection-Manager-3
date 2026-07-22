#include "ccm/ui/PokemonGameView.hpp"

#include "ccm/ui/CardEditModalGuard.hpp"
#include "ccm/ui/PokemonCardEditDialog.hpp"
#include "ccm/ui/PokemonCardListPanel.hpp"
#include "ccm/ui/PokemonSelectedCardPanel.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/msgdlg.h>
#include <wx/window.h>

#include <optional>
#include <string>

namespace ccm::ui {

PokemonGameView::PokemonGameView(ConfigService&                         config,
                                 CollectionService<PokemonCard>&        collection,
                                 SetService&                            sets,
                                 ImageService&                          images,
                                 CardPreviewService&                    cardPreview,
                                 IGameModule&                           module)
    : config_(config),
      collection_(collection),
      sets_(sets),
      images_(images),
      cardPreview_(cardPreview),
      module_(module) {}

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

void PokemonGameView::refreshCollection() {
    if (listPanel_ == nullptr) return;
    auto loaded = collection_.list(Game::Pokemon);
    if (!loaded) {
        showThemedMessageDialog(nullptr, "Failed to load Pokemon collection: " + loaded.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    listPanel_->setCards(std::move(loaded).value());
    listPanel_->activateSelection();
    if (selectedPanel_) selectedPanel_->setCard(listPanel_->selected());
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
    refreshCollection();
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
    auto westOut = sets_.updateSets(Game::Pokemon);
    auto asiaOut = sets_.updateSets(Game::JapanesePokemon);

    if (westOut) {
        setsCacheWest_ = westOut.value();
    }
    if (asiaOut) {
        setsCacheAsia_ = asiaOut.value();
    }

    if (!westOut && !asiaOut) {
        showThemedMessageDialog(
            parentWindow,
            "Failed to update West sets: " + westOut.error() +
                "\nFailed to update Asia sets: " + asiaOut.error(),
            "Error", wxOK | wxICON_ERROR);
        return "Update failed";
    }
    if (!westOut) {
        showThemedMessageDialog(
            parentWindow,
            "Updated " + std::to_string(asiaOut.value().size()) +
                " Asia Pokemon sets, but West failed: " + westOut.error(),
            "Sets partially updated", wxOK | wxICON_WARNING);
        return "Pokemon sets partially updated.";
    }
    if (!asiaOut) {
        showThemedMessageDialog(
            parentWindow,
            "Updated " + std::to_string(westOut.value().size()) +
                " West Pokemon sets, but Asia failed: " + asiaOut.error(),
            "Sets partially updated", wxOK | wxICON_WARNING);
        return "Pokemon sets partially updated.";
    }

    showThemedMessageDialog(
        parentWindow,
        "Updated " + std::to_string(westOut.value().size()) + " West and " +
            std::to_string(asiaOut.value().size()) + " Asia Pokemon sets.",
        "Sets updated", wxOK | wxICON_INFORMATION);
    return "Pokemon sets updated.";
}

void PokemonGameView::setFilter(std::string_view filter) {
    if (listPanel_) listPanel_->setFilter(filter);
}

void PokemonGameView::applyTheme(const ThemePalette& palette) {
    if (listPanel_)     listPanel_->applyTheme(palette);
    if (selectedPanel_) selectedPanel_->applyTheme(palette);
}

}  // namespace ccm::ui
