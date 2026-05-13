#include "ccm/ui/YuGiOhGameView.hpp"

#include "ccm/ui/CardEditModalGuard.hpp"
#include "ccm/ui/YuGiOhCardEditDialog.hpp"
#include "ccm/ui/YuGiOhCardListPanel.hpp"
#include "ccm/ui/YuGiOhSelectedCardPanel.hpp"
#include "ccm/ui/Theme.hpp"

#include <wx/msgdlg.h>
#include <wx/window.h>

#include <optional>
#include <algorithm>
#include <string>

namespace ccm::ui {

YuGiOhGameView::YuGiOhGameView(ConfigService&                        config,
                               CollectionService<YuGiOhCard>&        collection,
                               SetService&                           sets,
                               ImageService&                         images,
                               CardPreviewService&                   cardPreview,
                               IGameModule&                          module)
    : config_(config),
      collection_(collection),
      sets_(sets),
      images_(images),
      cardPreview_(cardPreview),
      module_(module) {}

void YuGiOhGameView::ensureSetsLoaded() {
    if (attemptedInitialSetLoad_) return;
    attemptedInitialSetLoad_ = true;

    auto cached = sets_.getSets(Game::YuGiOh);
    if (cached) {
        setsCache_ = std::move(cached).value();
        std::sort(setsCache_.begin(), setsCache_.end(),
                  [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
        if (!setsCache_.empty()) return;
    } else {
        setsCache_.clear();
    }

    auto refreshed = sets_.updateSets(Game::YuGiOh);
    if (refreshed) {
        setsCache_ = std::move(refreshed).value();
        std::sort(setsCache_.begin(), setsCache_.end(),
                  [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
    }
}

wxPanel* YuGiOhGameView::listPanel(wxWindow* parent) {
    if (listPanel_ == nullptr) {
        listPanel_ = new YuGiOhCardListPanel(parent);
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

wxPanel* YuGiOhGameView::selectedPanel(wxWindow* parent) {
    if (selectedPanel_ == nullptr) {
        selectedPanel_ = new YuGiOhSelectedCardPanel(parent, images_, cardPreview_);
    }
    return selectedPanel_;
}

void YuGiOhGameView::refreshCollection() {
    if (listPanel_ == nullptr) return;
    auto loaded = collection_.list(Game::YuGiOh);
    if (!loaded) {
        showThemedMessageDialog(nullptr, "Failed to load Yu-Gi-Oh! collection: " + loaded.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    listPanel_->setCards(std::move(loaded).value());
    listPanel_->activateSelection();
    if (selectedPanel_) selectedPanel_->setCard(listPanel_->selected());
}

const std::vector<Set>& YuGiOhGameView::setsForDialog() {
    ensureSetsLoaded();
    if (!setsCache_.empty()) return setsCache_;
    auto loaded = sets_.getSets(Game::YuGiOh);
    if (loaded) {
        setsCache_ = std::move(loaded).value();
        std::sort(setsCache_.begin(), setsCache_.end(),
                  [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
    }
    else        setsCache_.clear();
    return setsCache_;
}

void YuGiOhGameView::onAddCard(wxWindow* parentWindow) {
    if (cardEditModalIsActive()) {
        showThemedMessageDialog(parentWindow, wxString::FromUTF8(kCardEditModalBlockedUtf8),
                                wxString::FromUTF8("Add card"), wxOK | wxICON_INFORMATION);
        return;
    }
    YuGiOhCard fresh;
    fresh.amount = 1;
    fresh.language = Language::English;
    fresh.condition = Condition::NearMint;

    YuGiOhCardEditDialog dlg(parentWindow, images_, sets_, cardPreview_, EditMode::Create, fresh,
                             &setsForDialog());
    themeModalDialog(&dlg, config_.current().theme);
    CardEditModalGuard modalGuard;
    if (dlg.ShowModal() != wxID_OK) return;

    auto added = collection_.add(Game::YuGiOh, dlg.card());
    if (!added) {
        showThemedMessageDialog(parentWindow, "Failed to add card: " + added.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }

    YuGiOhCard persisted = dlg.card();
    persisted.id = added.value();
    const std::string setNameForImage = persisted.set.id.empty()
                                            ? persisted.set.name
                                            : persisted.set.id;
    auto normalized = images_.normalizeNamesForPersistedCard(
        Game::YuGiOh, persisted.id, setNameForImage, persisted.name, persisted.images);
    if (normalized) {
        if (normalized.value() != persisted.images) {
            persisted.images = std::move(normalized).value();
            auto updated = collection_.update(Game::YuGiOh, persisted);
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

void YuGiOhGameView::onEditCard(wxWindow* parentWindow) {
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
    YuGiOhCardEditDialog dlg(parentWindow, images_, sets_, cardPreview_, EditMode::Edit, *sel,
                             &setsForDialog());
    themeModalDialog(&dlg, config_.current().theme);
    CardEditModalGuard modalGuard;
    if (dlg.ShowModal() != wxID_OK) return;
    auto updated = collection_.update(Game::YuGiOh, dlg.card());
    if (!updated) {
        showThemedMessageDialog(parentWindow, "Failed to update card: " + updated.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    refreshCollection();
}

void YuGiOhGameView::onDeleteCard(wxWindow* parentWindow) {
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
    auto removed = collection_.remove(Game::YuGiOh, sel->id);
    if (!removed) {
        showThemedMessageDialog(parentWindow, "Failed to delete card: " + removed.error(),
                                "Error", wxOK | wxICON_ERROR);
        return;
    }
    refreshCollection();
}

std::string YuGiOhGameView::onUpdateSets(wxWindow* parentWindow) {
    auto out = sets_.updateSets(Game::YuGiOh);
    if (!out) {
        showThemedMessageDialog(parentWindow, "Failed to update sets: " + out.error(),
                                "Error", wxOK | wxICON_ERROR);
        return "Update failed";
    }
    setsCache_ = out.value();
    std::sort(setsCache_.begin(), setsCache_.end(),
              [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
    showThemedMessageDialog(parentWindow, "Updated " + std::to_string(out.value().size()) + " Yu-Gi-Oh! sets.",
                            "Sets updated", wxOK | wxICON_INFORMATION);
    return "Yu-Gi-Oh! sets updated.";
}

void YuGiOhGameView::setFilter(std::string_view filter) {
    if (listPanel_) listPanel_->setFilter(filter);
}

void YuGiOhGameView::applyTheme(const ThemePalette& palette) {
    if (listPanel_)     listPanel_->applyTheme(palette);
    if (selectedPanel_) selectedPanel_->applyTheme(palette);
}

}  // namespace ccm::ui
