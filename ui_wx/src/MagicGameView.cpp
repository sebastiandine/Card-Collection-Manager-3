#include "ccm/ui/MagicGameView.hpp"

#include "ccm/ui/MagicCardEditDialog.hpp"
#include "ccm/ui/MagicCardListPanel.hpp"
#include "ccm/ui/MagicSelectedCardPanel.hpp"

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
        wxMessageBox("Failed to load Magic collection: " + loaded.error(),
                     "Error", wxOK | wxICON_ERROR);
        return;
    }
    listPanel_->setCards(std::move(loaded).value());
    if (selectedPanel_) selectedPanel_->setCard(listPanel_->selected());
}

const std::vector<Set>& MagicGameView::setsForDialog() {
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
    {
        const Theme currentTheme = config_.current().theme;
        const ThemePalette palette = paletteForTheme(currentTheme);
        applyThemeToWindowTree(&dlg, palette, currentTheme);
        dlg.SetBackgroundColour(palette.panelBg);
        dlg.SetForegroundColour(palette.text);
    }
    if (dlg.ShowModal() != wxID_OK) return;

    auto added = collection_.add(Game::Magic, dlg.card());
    if (!added) {
        wxMessageBox("Failed to add card: " + added.error(),
                     "Error", wxOK | wxICON_ERROR, parentWindow);
        return;
    }
    refreshCollection();
}

void MagicGameView::onEditCard(wxWindow* parentWindow) {
    if (listPanel_ == nullptr) return;
    auto sel = listPanel_->selected();
    if (!sel) {
        wxMessageBox("Select a card first.", "Edit", wxOK | wxICON_INFORMATION, parentWindow);
        return;
    }
    MagicCardEditDialog dlg(parentWindow, images_, sets_, EditMode::Edit, *sel,
                            &setsForDialog());
    {
        const Theme currentTheme = config_.current().theme;
        const ThemePalette palette = paletteForTheme(currentTheme);
        applyThemeToWindowTree(&dlg, palette, currentTheme);
        dlg.SetBackgroundColour(palette.panelBg);
        dlg.SetForegroundColour(palette.text);
    }
    if (dlg.ShowModal() != wxID_OK) return;
    auto updated = collection_.update(Game::Magic, dlg.card());
    if (!updated) {
        wxMessageBox("Failed to update card: " + updated.error(),
                     "Error", wxOK | wxICON_ERROR, parentWindow);
        return;
    }
    refreshCollection();
}

void MagicGameView::onDeleteCard(wxWindow* parentWindow) {
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
    auto removed = collection_.remove(Game::Magic, sel->id);
    if (!removed) {
        wxMessageBox("Failed to delete card: " + removed.error(),
                     "Error", wxOK | wxICON_ERROR, parentWindow);
        return;
    }
    refreshCollection();
}

std::string MagicGameView::onUpdateSets(wxWindow* parentWindow) {
    auto out = sets_.updateSets(Game::Magic);
    if (!out) {
        wxMessageBox("Failed to update sets: " + out.error(),
                     "Error", wxOK | wxICON_ERROR, parentWindow);
        return "Update failed";
    }
    setsCache_ = out.value();
    wxMessageBox("Updated " + std::to_string(out.value().size()) + " Magic sets.",
                 "Sets updated", wxOK | wxICON_INFORMATION, parentWindow);
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
