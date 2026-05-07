#pragma once

// IGameView: per-game UI bundle that `MainFrame` swaps in/out when the user
// switches games. Each implementation owns its typed list panel + selected
// panel + Add/Edit/Delete dialogs and the cached set list. Common services
// (config, sets, images, card preview) come from the shared `AppContext`,
// so a new game implementation does not need its own copy of any of them.
//
// New games extend this interface — see `MagicGameView` and
// `PokemonGameView` for the canonical patterns.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/ui/Theme.hpp"

#include <string>
#include <string_view>
#include <vector>

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

    // Reload the active collection from disk and refresh the panels. The
    // selected card is preserved when possible.
    virtual void refreshCollection() = 0;

    // Toolbar actions. `parentWindow` is the dialog owner for any modal we
    // open (typically the `MainFrame`).
    virtual void onAddCard(wxWindow* parentWindow) = 0;
    virtual void onEditCard(wxWindow* parentWindow) = 0;
    virtual void onDeleteCard(wxWindow* parentWindow) = 0;

    // Sets menu action ("Update Magic" / "Update Pokemon"). Returns the
    // user-visible status string for the parent's status bar.
    virtual std::string onUpdateSets(wxWindow* parentWindow) = 0;

    // Forwarded by `MainFrame` whenever the filter input changes.
    virtual void setFilter(std::string_view filter) = 0;

    // Apply the active palette to all panels owned by this view.
    virtual void applyTheme(const ThemePalette& palette) = 0;

    // The Sets menu label suffix ("Magic" / "Pokemon"), used for the
    // dynamically built "Update <name>" menu entry.
    [[nodiscard]] virtual std::string updateSetsMenuLabel() const = 0;
};

}  // namespace ccm::ui
