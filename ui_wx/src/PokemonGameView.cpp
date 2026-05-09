#include "ccm/ui/PokemonGameView.hpp"

#include "ccm/ui/PokemonCardEditDialog.hpp"
#include "ccm/ui/PokemonCardListPanel.hpp"
#include "ccm/ui/PokemonSelectedCardPanel.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/msgdlg.h>

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

    auto cached = sets_.getSets(Game::Pokemon);
    if (cached) {
        setsCache_ = std::move(cached).value();
        if (!setsCache_.empty()) return;
    } else {
        setsCache_.clear();
    }

    auto refreshed = sets_.updateSets(Game::Pokemon);
    if (refreshed) {
        setsCache_ = std::move(refreshed).value();
    }
}

wxPanel* PokemonGameView::listPanel(wxWindow* parent) {
    if (listPanel_ == nullptr) {
        listPanel_ = new PokemonCardListPanel(parent);
        listPanel_->Bind(EVT_CARD_SELECTED, [this](wxCommandEvent&) {
            if (selectedPanel_ != nullptr && listPanel_ != nullptr) {
                selectedPanel_->setCard(listPanel_->selected());
            }
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

const std::vector<Set>& PokemonGameView::setsForDialog() {
    ensureSetsLoaded();
    if (!setsCache_.empty()) return setsCache_;
    auto loaded = sets_.getSets(Game::Pokemon);
    if (loaded) setsCache_ = std::move(loaded).value();
    else        setsCache_.clear();
    return setsCache_;
}

void PokemonGameView::onAddCard(wxWindow* parentWindow) {
    PokemonCard fresh;
    fresh.amount = 1;
    fresh.language = Language::English;
    fresh.condition = Condition::NearMint;

    PokemonCardEditDialog dlg(parentWindow, images_, sets_, EditMode::Create, fresh,
                              &setsForDialog());
    themeModalDialog(&dlg, config_.current().theme);
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
    PokemonCardEditDialog dlg(parentWindow, images_, sets_, EditMode::Edit, *sel,
                              &setsForDialog());
    themeModalDialog(&dlg, config_.current().theme);
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
    auto out = sets_.updateSets(Game::Pokemon);
    if (!out) {
        showThemedMessageDialog(parentWindow, "Failed to update sets: " + out.error(),
                                "Error", wxOK | wxICON_ERROR);
        return "Update failed";
    }
    setsCache_ = out.value();
    showThemedMessageDialog(parentWindow, "Updated " + std::to_string(out.value().size()) + " Pokemon sets.",
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
