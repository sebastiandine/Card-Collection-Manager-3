#include "ccm/ui/MagicGameView.hpp"

#include "ccm/ui/MagicCardEditDialog.hpp"
#include "ccm/ui/MagicCardListPanel.hpp"
#include "ccm/ui/MagicSelectedCardPanel.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/msgdlg.h>

#include <optional>
#include <string>

namespace ccm::ui {

MagicGameView::MagicGameView(ConfigService&                       config,
                             CollectionService<MagicCard>&        collection,
                             SetService&                          sets,
                             ImageService&                        images,
                             CardPreviewService&                  cardPreview,
                             IGameModule&                         module)
    : config_(config),
      collection_(collection),
      sets_(sets),
      images_(images),
      cardPreview_(cardPreview),
      module_(module) {}

void MagicGameView::ensureSetsLoaded() {
    if (attemptedInitialSetLoad_) return;
    attemptedInitialSetLoad_ = true;

    auto cached = sets_.getSets(Game::Magic);
    if (cached) {
        setsCache_ = std::move(cached).value();
        if (!setsCache_.empty()) return;
    } else {
        setsCache_.clear();
    }

    auto refreshed = sets_.updateSets(Game::Magic);
    if (refreshed) {
        setsCache_ = std::move(refreshed).value();
    }
}

wxPanel* MagicGameView::listPanel(wxWindow* parent) {
    if (listPanel_ == nullptr) {
        listPanel_ = new MagicCardListPanel(parent);
        // Selection in the list -> push the typed card to the selected panel.
        // Binding here (in the view, not in MainFrame) keeps the typed wiring
        // local to the per-game implementation - MainFrame only sees IGameView.
        listPanel_->Bind(EVT_CARD_SELECTED, [this](wxCommandEvent&) {
            if (selectedPanel_ != nullptr && listPanel_ != nullptr) {
                selectedPanel_->setCard(listPanel_->selected());
            }
        });
    }
    return listPanel_;
}

wxPanel* MagicGameView::selectedPanel(wxWindow* parent) {
    if (selectedPanel_ == nullptr) {
        selectedPanel_ = new MagicSelectedCardPanel(parent, images_, cardPreview_);
    }
    return selectedPanel_;
}

void MagicGameView::refreshCollection() {
    if (listPanel_ == nullptr) return;
    auto loaded = collection_.list(Game::Magic);
    if (!loaded) {
        showThemedMessageDialog(nullptr, "Failed to load Magic collection: " + loaded.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    listPanel_->setCards(std::move(loaded).value());
    listPanel_->activateSelection();
    if (selectedPanel_) selectedPanel_->setCard(listPanel_->selected());
}

const std::vector<Set>& MagicGameView::setsForDialog() {
    ensureSetsLoaded();
    if (!setsCache_.empty()) return setsCache_;
    auto loaded = sets_.getSets(Game::Magic);
    if (loaded) setsCache_ = std::move(loaded).value();
    else        setsCache_.clear();
    return setsCache_;
}

void MagicGameView::onAddCard(wxWindow* parentWindow) {
    MagicCard fresh;
    fresh.amount = 1;
    fresh.language = Language::English;
    fresh.condition = Condition::NearMint;

    MagicCardEditDialog dlg(parentWindow, images_, sets_, EditMode::Create, fresh,
                            &setsForDialog());
    themeModalDialog(&dlg, config_.current().theme);
    if (dlg.ShowModal() != wxID_OK) return;

    auto added = collection_.add(Game::Magic, dlg.card());
    if (!added) {
        showThemedMessageDialog(parentWindow, "Failed to add card: " + added.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }

    MagicCard persisted = dlg.card();
    persisted.id = added.value();
    auto normalized = images_.normalizeNamesForPersistedCard(
        Game::Magic, persisted.id, persisted.set.name, persisted.name, persisted.images);
    if (normalized) {
        if (normalized.value() != persisted.images) {
            persisted.images = std::move(normalized).value();
            auto updated = collection_.update(Game::Magic, persisted);
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

void MagicGameView::onEditCard(wxWindow* parentWindow) {
    if (listPanel_ == nullptr) return;
    auto sel = listPanel_->selected();
    if (!sel) {
        showThemedMessageDialog(parentWindow, "Select a card first.", "Edit", wxOK | wxICON_INFORMATION);
        return;
    }
    MagicCardEditDialog dlg(parentWindow, images_, sets_, EditMode::Edit, *sel,
                            &setsForDialog());
    themeModalDialog(&dlg, config_.current().theme);
    if (dlg.ShowModal() != wxID_OK) return;
    auto updated = collection_.update(Game::Magic, dlg.card());
    if (!updated) {
        showThemedMessageDialog(parentWindow, "Failed to update card: " + updated.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    refreshCollection();
}

void MagicGameView::onDeleteCard(wxWindow* parentWindow) {
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
    auto removed = collection_.remove(Game::Magic, sel->id);
    if (!removed) {
        showThemedMessageDialog(parentWindow, "Failed to delete card: " + removed.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    refreshCollection();
}

std::string MagicGameView::onUpdateSets(wxWindow* parentWindow) {
    auto out = sets_.updateSets(Game::Magic);
    if (!out) {
        showThemedMessageDialog(parentWindow, "Failed to update sets: " + out.error(),
                                "Error", wxOK | wxICON_ERROR);
        return "Update failed";
    }
    setsCache_ = out.value();
    showThemedMessageDialog(parentWindow, "Updated " + std::to_string(out.value().size()) + " Magic sets.",
                            "Sets updated", wxOK | wxICON_INFORMATION);
    return "Magic sets updated.";
}

void MagicGameView::setFilter(std::string_view filter) {
    if (listPanel_) listPanel_->setFilter(filter);
}

void MagicGameView::applyTheme(const ThemePalette& palette) {
    if (listPanel_)     listPanel_->applyTheme(palette);
    if (selectedPanel_) selectedPanel_->applyTheme(palette);
}

}  // namespace ccm::ui
