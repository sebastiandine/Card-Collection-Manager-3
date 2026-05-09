#include "ccm/ui/PokemonCardListPanel.hpp"

#include "ccm/services/CardFilter.hpp"
#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

PokemonCardListPanel::PokemonCardListPanel(wxWindow* parent)
    : BaseCardListPanel<PokemonCard, PokemonSortColumn>(parent) {
    buildLayout();
}

std::vector<PokemonCardListPanel::TextColumnSpec>
PokemonCardListPanel::declareTextColumns() const {
    // Order mirrors the Magic table for visual parity. Pokemon adds two
    // additional flag-icon columns (Holo, FirstEdition) but keeps the same
    // leading text-column shape. setNo is not displayed in the table; it
    // appears in the detail panel and is searchable through the filter.
    return {
        {"Name",      220, wxLIST_FORMAT_LEFT,  PokemonSortColumn::Name},
        {"Set",       180, wxLIST_FORMAT_LEFT,  PokemonSortColumn::SetReleaseDate},
        {"Amount",    70,  wxLIST_FORMAT_RIGHT, PokemonSortColumn::Amount},
        {"Condition", 100, wxLIST_FORMAT_LEFT,  PokemonSortColumn::Condition},
        {"Language",  100, wxLIST_FORMAT_LEFT,  PokemonSortColumn::Language},
        {"Note",      220, wxLIST_FORMAT_LEFT,  PokemonSortColumn::Note},
    };
}

std::vector<PokemonCardListPanel::IconColumnSpec>
PokemonCardListPanel::declareIconColumns() const {
    constexpr int kFlagColWidth = 36;
    return {
        {kSvgHolo,         kFlagColWidth, PokemonSortColumn::Holo},
        {kSvgFirstEdition, kFlagColWidth, PokemonSortColumn::FirstEdition},
        {kSvgSigned,       kFlagColWidth, PokemonSortColumn::Signed},
        {kSvgAltered,      kFlagColWidth, PokemonSortColumn::Altered},
    };
}

std::string PokemonCardListPanel::renderTextCell(const PokemonCard& card,
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

bool PokemonCardListPanel::isIconColumnSet(const PokemonCard& card,
                                           std::size_t idx) const {
    switch (idx) {
    case 0: return card.holo;
    case 1: return card.firstEdition;
    case 2: return card.signed_;
    case 3: return card.altered;
    }
    return false;
}

void PokemonCardListPanel::sortBy(PokemonSortColumn column, bool ascending) {
    sortPokemonCards(mutableCards(), column, ascending);
}

bool PokemonCardListPanel::matchesFilter(const PokemonCard& card,
                                         std::string_view filter) const {
    return matchesPokemonFilter(card, filter);
}

}  // namespace ccm::ui
