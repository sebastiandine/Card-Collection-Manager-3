#include "ccm/ui/JapanesePokemonCardListPanel.hpp"

#include "ccm/services/CardFilter.hpp"
#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

JapanesePokemonCardListPanel::JapanesePokemonCardListPanel(wxWindow* parent)
    : BaseCardListPanel<JapanesePokemonCard, JapanesePokemonSortColumn>(parent) {
    buildLayout();
}

std::vector<JapanesePokemonCardListPanel::TextColumnSpec>
JapanesePokemonCardListPanel::declareTextColumns() const {
    // Order mirrors the Magic table for visual parity. Pokemon adds two
    // additional flag-icon columns (Holo, FirstEdition) but keeps the same
    // leading text-column shape. setNo is not displayed in the table; it
    // appears in the detail panel and is searchable through the filter.
    return {
        {"Name",      220, wxLIST_FORMAT_LEFT,  JapanesePokemonSortColumn::Name},
        {"Set",       180, wxLIST_FORMAT_LEFT,  JapanesePokemonSortColumn::SetReleaseDate},
        {"Amount",    70,  wxLIST_FORMAT_RIGHT, JapanesePokemonSortColumn::Amount},
        {"Condition", 100, wxLIST_FORMAT_LEFT,  JapanesePokemonSortColumn::Condition},
        {"Language",  100, wxLIST_FORMAT_LEFT,  JapanesePokemonSortColumn::Language},
        {"Note",      220, wxLIST_FORMAT_LEFT,  JapanesePokemonSortColumn::Note},
    };
}

std::vector<JapanesePokemonCardListPanel::IconColumnSpec>
JapanesePokemonCardListPanel::declareIconColumns() const {
    constexpr int kFlagColWidth = 36;
    return {
        {kSvgHolo,         kFlagColWidth, JapanesePokemonSortColumn::Holo},
        {kSvgFirstEdition, kFlagColWidth, JapanesePokemonSortColumn::FirstEdition},
        {kSvgSigned,       kFlagColWidth, JapanesePokemonSortColumn::Signed},
        {kSvgAltered,      kFlagColWidth, JapanesePokemonSortColumn::Altered},
    };
}

std::string JapanesePokemonCardListPanel::renderTextCell(const JapanesePokemonCard& card,
                                                 std::size_t idx) const {
    switch (idx) {
    case 0: return card.name;
    case 1: return card.set.name;
    case 2: return std::to_string(card.amount);
    case 3: return std::string(to_string(card.condition));
    case 4: return std::string(to_string(card.language));
    case 5: return card.note;
    }
    return {};
}

bool JapanesePokemonCardListPanel::isIconColumnSet(const JapanesePokemonCard& card,
                                           std::size_t idx) const {
    switch (idx) {
    case 0: return card.holo;
    case 1: return card.firstEdition;
    case 2: return card.signed_;
    case 3: return card.altered;
    }
    return false;
}

void JapanesePokemonCardListPanel::sortBy(JapanesePokemonSortColumn column, bool ascending) {
    sortJapanesePokemonCards(mutableCards(), column, ascending);
}

bool JapanesePokemonCardListPanel::matchesFilter(const JapanesePokemonCard& card,
                                         std::string_view filter) const {
    return matchesJapanesePokemonFilter(card, filter);
}

}  // namespace ccm::ui
