#include "ccm/ui/DigiBattle99GameView.hpp"

#include "ccm/ui/CardEditModalGuard.hpp"
#include "ccm/ui/DigiBattle99CardEditDialog.hpp"
#include "ccm/ui/DigiBattle99CardListPanel.hpp"
#include "ccm/ui/DigiBattle99SelectedCardPanel.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/msgdlg.h>
#include <wx/window.h>

#include <optional>
#include <string>

namespace ccm::ui {

DigiBattle99GameView::DigiBattle99GameView(ConfigService&                         config,
                                           CollectionService<DigiBattle99Card>&   collection,
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

void DigiBattle99GameView::refreshCollection() {
    if (listPanel_ == nullptr) return;
    auto loaded = collection_.list(Game::DigiBattle99);
    if (!loaded) {
        showThemedMessageDialog(
            nullptr,
            "Failed to load Digimon (Digi-Battle) collection: " + loaded.error(),
            "Error", wxOK | wxICON_ERROR);
        return;
    }
    listPanel_->setCards(std::move(loaded).value());
    listPanel_->activateSelection();
    if (selectedPanel_) selectedPanel_->setCard(listPanel_->selected());
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
    refreshCollection();
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
    auto out = sets_.updateSets(Game::DigiBattle99);
    if (!out) {
        showThemedMessageDialog(parentWindow, "Failed to update sets: " + out.error(),
                                "Error", wxOK | wxICON_ERROR);
        return "Update failed";
    }
    setsCache_ = out.value();
    showThemedMessageDialog(
        parentWindow,
        "Updated " + std::to_string(out.value().size()) + " Digimon (Digi-Battle) sets.",
        "Sets updated", wxOK | wxICON_INFORMATION);
    return "Digimon (Digi-Battle) sets updated.";
}

void DigiBattle99GameView::setFilter(std::string_view filter) {
    if (listPanel_) listPanel_->setFilter(filter);
}

void DigiBattle99GameView::applyTheme(const ThemePalette& palette) {
    if (listPanel_)     listPanel_->applyTheme(palette);
    if (selectedPanel_) selectedPanel_->applyTheme(palette);
}

}  // namespace ccm::ui
