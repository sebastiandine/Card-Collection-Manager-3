#include "ccm/ui/YuGiOhCardListPanel.hpp"

#include "ccm/services/CardFilter.hpp"
#include "ccm/ui/SvgIcons.hpp"
#include "ccm/util/YuGiOhPrintingSlot.hpp"

#include <string>

namespace ccm::ui {

YuGiOhCardListPanel::YuGiOhCardListPanel(wxWindow* parent)
    : BaseCardListPanel<YuGiOhCard, YuGiOhSortColumn>(parent) {
    buildLayout();
}

std::vector<YuGiOhCardListPanel::TextColumnSpec>
YuGiOhCardListPanel::declareTextColumns() const {
    return {
        {"Name",      200, wxLIST_FORMAT_LEFT,   YuGiOhSortColumn::Name},
        {"Set",       160, wxLIST_FORMAT_LEFT,   YuGiOhSortColumn::SetReleaseDate},
        {"Amount",    70,  wxLIST_FORMAT_RIGHT,  YuGiOhSortColumn::Amount},
        {"Rarity",    90,  wxLIST_FORMAT_LEFT,   YuGiOhSortColumn::Rarity},
        {"Condition", 100, wxLIST_FORMAT_LEFT,   YuGiOhSortColumn::Condition},
        {"Language",  100, wxLIST_FORMAT_LEFT,   YuGiOhSortColumn::Language},
        {"Note",      180, wxLIST_FORMAT_LEFT,   YuGiOhSortColumn::Note},
    };
}

std::vector<YuGiOhCardListPanel::IconColumnSpec>
YuGiOhCardListPanel::declareIconColumns() const {
    constexpr int kFlagColWidth = 36;
    return {
        {kSvgFirstEdition, kFlagColWidth, YuGiOhSortColumn::FirstEdition},
        {kSvgSigned,       kFlagColWidth, YuGiOhSortColumn::Signed},
        {kSvgAltered,      kFlagColWidth, YuGiOhSortColumn::Altered},
    };
}

std::string YuGiOhCardListPanel::renderTextCell(const YuGiOhCard& card,
                                                std::size_t idx) const {
    switch (idx) {
    case 0: return card.name;
    case 1: return card.set.name;
    case 2: return std::to_string(card.amount);
    case 3: return ygoRarityShortCode(card.rarity);
    case 4: return std::string(to_string(card.condition));
    case 5: return std::string(to_string(card.language));
    case 6: return card.note;
    }
    return {};
}

bool YuGiOhCardListPanel::isIconColumnSet(const YuGiOhCard& card,
                                          std::size_t idx) const {
    switch (idx) {
    case 0: return card.firstEdition;
    case 1: return card.signed_;
    case 2: return card.altered;
    }
    return false;
}

void YuGiOhCardListPanel::sortBy(YuGiOhSortColumn column, bool ascending) {
    sortYuGiOhCards(mutableCards(), column, ascending);
}

bool YuGiOhCardListPanel::matchesFilter(const YuGiOhCard& card,
                                        std::string_view filter) const {
    return matchesYuGiOhFilter(card, filter);
}

}  // namespace ccm::ui
