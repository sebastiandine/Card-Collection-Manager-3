#include "ccm/ui/YuGiOhBandaiCardListPanel.hpp"

#include "ccm/services/CardFilter.hpp"
#include "ccm/ui/SvgIcons.hpp"

#include <string>

namespace ccm::ui {

YuGiOhBandaiCardListPanel::YuGiOhBandaiCardListPanel(wxWindow* parent)
    : BaseCardListPanel<YuGiOhBandaiCard, YuGiOhBandaiSortColumn>(parent) {
    buildLayout();
}

std::vector<YuGiOhBandaiCardListPanel::TextColumnSpec>
YuGiOhBandaiCardListPanel::declareTextColumns() const {
    return {
        {"Name",      200, wxLIST_FORMAT_LEFT,  YuGiOhBandaiSortColumn::Name},
        {"Set",       150, wxLIST_FORMAT_LEFT,  YuGiOhBandaiSortColumn::SetReleaseDate},
        {"No.",       70,  wxLIST_FORMAT_LEFT,  YuGiOhBandaiSortColumn::SetNo},
        {"Rarity",    100, wxLIST_FORMAT_LEFT,  YuGiOhBandaiSortColumn::Rarity},
        {"Amount",    70,  wxLIST_FORMAT_RIGHT, YuGiOhBandaiSortColumn::Amount},
        {"Condition", 100, wxLIST_FORMAT_LEFT,  YuGiOhBandaiSortColumn::Condition},
        {"Language",  100, wxLIST_FORMAT_LEFT,  YuGiOhBandaiSortColumn::Language},
        {"Note",      180, wxLIST_FORMAT_LEFT,  YuGiOhBandaiSortColumn::Note},
    };
}

std::vector<YuGiOhBandaiCardListPanel::IconColumnSpec>
YuGiOhBandaiCardListPanel::declareIconColumns() const {
    constexpr int kFlagColWidth = 36;
    return {
        {kSvgHolo,    kFlagColWidth, YuGiOhBandaiSortColumn::Holo},
        {kSvgSigned,  kFlagColWidth, YuGiOhBandaiSortColumn::Signed},
        {kSvgAltered, kFlagColWidth, YuGiOhBandaiSortColumn::Altered},
    };
}

std::string YuGiOhBandaiCardListPanel::renderTextCell(const YuGiOhBandaiCard& card,
                                                      std::size_t idx) const {
    switch (idx) {
    case 0: return card.name;
    case 1: return card.set.name;
    case 2: return card.setNo;
    case 3: return card.rarity;
    case 4: return std::to_string(card.amount);
    case 5: return std::string(to_string(card.condition));
    case 6: return std::string(to_string(card.language));
    case 7: return card.note;
    }
    return {};
}

bool YuGiOhBandaiCardListPanel::isIconColumnSet(const YuGiOhBandaiCard& card,
                                                std::size_t idx) const {
    switch (idx) {
    case 0: return card.holo;
    case 1: return card.signed_;
    case 2: return card.altered;
    }
    return false;
}

void YuGiOhBandaiCardListPanel::sortBy(YuGiOhBandaiSortColumn column, bool ascending) {
    sortYuGiOhBandaiCards(mutableCards(), column, ascending);
}

bool YuGiOhBandaiCardListPanel::matchesFilter(const YuGiOhBandaiCard& card,
                                              std::string_view filter) const {
    return matchesYuGiOhBandaiFilter(card, filter);
}

}  // namespace ccm::ui
