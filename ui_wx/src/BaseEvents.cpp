// Definitions for events shared by the per-game UI templates. The events are
// declared in the corresponding base headers (BaseCardListPanel.hpp,
// BaseSelectedCardPanel.hpp) and defined exactly once here, so that template
// instantiations (Magic, Pokemon, ...) all use the same event type tag.
// EVT_CARD_ACTIVATED is declared alongside EVT_CARD_SELECTED in
// BaseCardListPanel.hpp.

#include "ccm/ui/BaseCardListPanel.hpp"
#include "ccm/ui/BaseSelectedCardPanel.hpp"

namespace ccm::ui {

wxDEFINE_EVENT(EVT_CARD_SELECTED, wxCommandEvent);
wxDEFINE_EVENT(EVT_CARD_ACTIVATED, wxCommandEvent);
wxDEFINE_EVENT(EVT_PREVIEW_STATUS, wxCommandEvent);

}  // namespace ccm::ui
