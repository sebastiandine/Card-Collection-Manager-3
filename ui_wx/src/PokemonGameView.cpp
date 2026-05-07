#include "ccm/ui/PokemonGameView.hpp"

#include "ccm/ui/PokemonCardEditDialog.hpp"
#include "ccm/ui/PokemonCardListPanel.hpp"
#include "ccm/ui/PokemonSelectedCardPanel.hpp"

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
        wxMessageBox("Failed to load Pokemon collection: " + loaded.error(),
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
    {
        const Theme currentTheme = config_.current().theme;
        const ThemePalette palette = paletteForTheme(currentTheme);
        applyThemeToWindowTree(&dlg, palette, currentTheme);
        dlg.SetBackgroundColour(palette.panelBg);
        dlg.SetForegroundColour(palette.text);
    }
    if (dlg.ShowModal() != wxID_OK) return;

    auto added = collection_.add(Game::Pokemon, dlg.card());
    if (!added) {
        wxMessageBox("Failed to add card: " + added.error(),
                     "Error", wxOK | wxICON_ERROR, parentWindow);
        return;
    }
    refreshCollection();
}

void PokemonGameView::onEditCard(wxWindow* parentWindow) {
    if (listPanel_ == nullptr) return;
    auto sel = listPanel_->selected();
    if (!sel) {
        wxMessageBox("Select a card first.", "Edit", wxOK | wxICON_INFORMATION, parentWindow);
        return;
    }
    PokemonCardEditDialog dlg(parentWindow, images_, sets_, EditMode::Edit, *sel,
                              &setsForDialog());
    {
        const Theme currentTheme = config_.current().theme;
        const ThemePalette palette = paletteForTheme(currentTheme);
        applyThemeToWindowTree(&dlg, palette, currentTheme);
        dlg.SetBackgroundColour(palette.panelBg);
        dlg.SetForegroundColour(palette.text);
    }
    if (dlg.ShowModal() != wxID_OK) return;
    auto updated = collection_.update(Game::Pokemon, dlg.card());
    if (!updated) {
        wxMessageBox("Failed to update card: " + updated.error(),
                     "Error", wxOK | wxICON_ERROR, parentWindow);
        return;
    }
    refreshCollection();
}

void PokemonGameView::onDeleteCard(wxWindow* parentWindow) {
    if (listPanel_ == nullptr) return;
    auto sel = listPanel_->selected();
    if (!sel) {
        wxMessageBox("Select a card first.", "Delete", wxOK | wxICON_INFORMATION, parentWindow);
        return;
    }
    if (wxMessageBox("Delete \"" + sel->name + "\"?",
                     "Confirm", wxYES_NO | wxICON_QUESTION, parentWindow) != wxYES) {
        return;
    }
    auto removed = collection_.remove(Game::Pokemon, sel->id);
    if (!removed) {
        wxMessageBox("Failed to delete card: " + removed.error(),
                     "Error", wxOK | wxICON_ERROR, parentWindow);
        return;
    }
    refreshCollection();
}

std::string PokemonGameView::onUpdateSets(wxWindow* parentWindow) {
    auto out = sets_.updateSets(Game::Pokemon);
    if (!out) {
        wxMessageBox("Failed to update sets: " + out.error(),
                     "Error", wxOK | wxICON_ERROR, parentWindow);
        return "Update failed";
    }
    setsCache_ = out.value();
    wxMessageBox("Updated " + std::to_string(out.value().size()) + " Pokemon sets.",
                 "Sets updated", wxOK | wxICON_INFORMATION, parentWindow);
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
