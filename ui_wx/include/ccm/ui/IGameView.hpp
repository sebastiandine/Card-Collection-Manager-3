#pragma once

// IGameView: per-game UI bundle that `MainFrame` swaps in/out when the user
// switches games. Each implementation owns its typed list panel + selected
// panel + Add/Edit/Delete dialogs and the cached set list. Common services
// (config, sets, images, card preview) come from the shared `AppContext`,
// so a new game implementation does not need its own copy of any of them.
//
// New games extend this interface — see `MagicGameView` (Single Cards |
// Deck Check notebook) and `PokemonGameView` (Single Cards | Set Completion)
// for the canonical patterns.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/ui/Theme.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class wxBitmapButton;
class wxPanel;
class wxWindow;

namespace ccm::ui {

class IGameView {
public:
    virtual ~IGameView() = default;

    [[nodiscard]] virtual Game        gameId() const noexcept = 0;
    [[nodiscard]] virtual std::string displayName() const = 0;

    // The two panels owned by this view. They are constructed lazily — the
    // first call must accept `parent` so the panels become children of the
    // splitter. Subsequent calls return the cached pointers.
    virtual wxPanel* listPanel(wxWindow* parent) = 0;
    virtual wxPanel* selectedPanel(wxWindow* parent) = 0;

    // When non-null, MainFrame mounts this as the sole content under the
    // toolbar instead of the shared selected|list splitter. Magic, Digimon,
    // Yu-Gi-Oh!, and Pokemon use this for in-game notebooks (Single Cards
    // plus Set Completion or Deck Check). Default: no custom host.
    virtual wxPanel* contentPanel(wxWindow* parent) {
        (void)parent;
        return nullptr;
    }

    // Non-constructing accessor so MainFrame can hide a previously mounted
    // content panel without forcing lazy creation for inactive games.
    [[nodiscard]] virtual wxPanel* contentPanelIfCreated() const noexcept {
        return nullptr;
    }

    // Games that own their layout via contentPanel must not have their
    // list/selected panels parented onto MainFrame's shared splitter.
    // Magic, Pokemon, Yu-Gi-Oh!, and Digimon all return true.
    [[nodiscard]] virtual bool hostsOwnLayout() const noexcept { return false; }

    // Reload the active collection from disk and refresh the panels. When
    // selectId is set, that card is selected if present (e.g. after Add);
    // otherwise the previously selected card is preserved when possible.
    virtual void refreshCollection(std::optional<std::uint32_t> selectId = std::nullopt) = 0;

    // Toolbar actions. `parentWindow` is the dialog owner for any modal we
    // open (typically the `MainFrame`).
    virtual void onAddCard(wxWindow* parentWindow) = 0;
    virtual void onEditCard(wxWindow* parentWindow) = 0;
    virtual void onDeleteCard(wxWindow* parentWindow) = 0;

    // Legacy hook for a MainFrame-owned Edit button. hostsOwnLayout games
    // (including Magic) ignore this and manage their own toolbar. Default no-op.
    virtual void attachSharedToolbarEdit(wxBitmapButton* edit) { (void)edit; }

    // Sets menu action ("Update Magic" / "Update Pokemon"). Returns the
    // user-visible status string for the parent's status bar.
    virtual std::string onUpdateSets(wxWindow* parentWindow) = 0;

    // Forwarded by `MainFrame` whenever the filter input changes.
    virtual void setFilter(std::string_view filter) = 0;

    // Move the card-list selection by `delta` rows (+1 / -1). Used when Up/Down
    // are pressed while the filter text box has focus.
    virtual void nudgeSelection(int delta) = 0;

    // Apply the active palette to all panels owned by this view.
    virtual void applyTheme(const ThemePalette& palette) = 0;

    // The Sets menu label suffix ("Magic" / "Pokemon"), used for the
    // dynamically built "Update <name>" menu entry.
    [[nodiscard]] virtual std::string updateSetsMenuLabel() const = 0;
};

}  // namespace ccm::ui
