#include "ccm/ui/DigiBattle99CardListPanel.hpp"

#include "ccm/services/CardFilter.hpp"
#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

DigiBattle99CardListPanel::DigiBattle99CardListPanel(wxWindow* parent)
    : BaseCardListPanel<DigiBattle99Card, DigiBattle99SortColumn>(parent) {
    buildLayout();
}

std::vector<DigiBattle99CardListPanel::TextColumnSpec>
DigiBattle99CardListPanel::declareTextColumns() const {
    return {
        {"Name",      220, wxLIST_FORMAT_LEFT,  DigiBattle99SortColumn::Name},
        {"Set",       180, wxLIST_FORMAT_LEFT,  DigiBattle99SortColumn::SetReleaseDate},
        {"Amount",    70,  wxLIST_FORMAT_RIGHT, DigiBattle99SortColumn::Amount},
        {"Condition", 100, wxLIST_FORMAT_LEFT,  DigiBattle99SortColumn::Condition},
        {"Language",  100, wxLIST_FORMAT_LEFT,  DigiBattle99SortColumn::Language},
        {"Note",      220, wxLIST_FORMAT_LEFT,  DigiBattle99SortColumn::Note},
    };
}

std::vector<DigiBattle99CardListPanel::IconColumnSpec>
DigiBattle99CardListPanel::declareIconColumns() const {
    constexpr int kFlagColWidth = 36;
    return {
        {kSvgHolo,         kFlagColWidth, DigiBattle99SortColumn::Holo},
        {kSvgFirstEdition, kFlagColWidth, DigiBattle99SortColumn::FirstEdition},
        {kSvgSigned,       kFlagColWidth, DigiBattle99SortColumn::Signed},
        {kSvgAltered,      kFlagColWidth, DigiBattle99SortColumn::Altered},
    };
}

std::string DigiBattle99CardListPanel::renderTextCell(const DigiBattle99Card& card,
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

bool DigiBattle99CardListPanel::isIconColumnSet(const DigiBattle99Card& card,
                                                std::size_t idx) const {
    switch (idx) {
    case 0: return card.holo;
    case 1: return card.firstEdition;
    case 2: return card.signed_;
    case 3: return card.altered;
    }
    return false;
}

void DigiBattle99CardListPanel::sortBy(DigiBattle99SortColumn column, bool ascending) {
    sortDigiBattle99Cards(mutableCards(), column, ascending);
}

bool DigiBattle99CardListPanel::matchesFilter(const DigiBattle99Card& card,
                                              std::string_view filter) const {
    return matchesDigiBattle99Filter(card, filter);
}

}  // namespace ccm::ui
