#include "ccm/ui/MagicCardListPanel.hpp"

#include "ccm/services/CardFilter.hpp"
#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

MagicCardListPanel::MagicCardListPanel(wxWindow* parent)
    : BaseCardListPanel<MagicCard, MagicSortColumn>(parent) {
    buildLayout();
}

std::vector<MagicCardListPanel::TextColumnSpec>
MagicCardListPanel::declareTextColumns() const {
    return {
        {"Name",      220, wxLIST_FORMAT_LEFT,  MagicSortColumn::Name},
        {"Set",       180, wxLIST_FORMAT_LEFT,  MagicSortColumn::SetReleaseDate},
        {"Amount",    70,  wxLIST_FORMAT_RIGHT, MagicSortColumn::Amount},
        {"Condition", 100, wxLIST_FORMAT_LEFT,  MagicSortColumn::Condition},
        {"Language",  100, wxLIST_FORMAT_LEFT,  MagicSortColumn::Language},
        // Trailing Note column, always last.
        {"Note",      220, wxLIST_FORMAT_LEFT,  MagicSortColumn::Note},
    };
}

std::vector<MagicCardListPanel::IconColumnSpec>
MagicCardListPanel::declareIconColumns() const {
    constexpr int kFlagColWidth = 36;
    return {
        {kSvgFoil,    kFlagColWidth, MagicSortColumn::Foil},
        {kSvgSigned,  kFlagColWidth, MagicSortColumn::Signed},
        {kSvgAltered, kFlagColWidth, MagicSortColumn::Altered},
    };
}

std::string MagicCardListPanel::renderTextCell(const MagicCard& card,
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

bool MagicCardListPanel::isIconColumnSet(const MagicCard& card,
                                         std::size_t idx) const {
    switch (idx) {
    case 0: return card.foil;
    case 1: return card.signed_;
    case 2: return card.altered;
    }
    return false;
}

void MagicCardListPanel::sortBy(MagicSortColumn column, bool ascending) {
    sortMagicCards(mutableCards(), column, ascending);
}

bool MagicCardListPanel::matchesFilter(const MagicCard& card,
                                       std::string_view filter) const {
    return matchesMagicFilter(card, filter);
}

}  // namespace ccm::ui
