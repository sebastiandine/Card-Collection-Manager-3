#pragma once

#include "ccm/domain/YuGiOhBandaiCard.hpp"
#include "ccm/services/CardSorter.hpp"
#include "ccm/ui/BaseCardListPanel.hpp"

namespace ccm::ui {

class YuGiOhBandaiCardListPanel final
    : public BaseCardListPanel<YuGiOhBandaiCard, YuGiOhBandaiSortColumn> {
public:
    explicit YuGiOhBandaiCardListPanel(wxWindow* parent);

protected:
    [[nodiscard]] std::vector<TextColumnSpec> declareTextColumns() const override;
    [[nodiscard]] std::vector<IconColumnSpec> declareIconColumns() const override;
    [[nodiscard]] std::string renderTextCell(const YuGiOhBandaiCard& card,
                                             std::size_t idx) const override;
    [[nodiscard]] bool isIconColumnSet(const YuGiOhBandaiCard& card,
                                       std::size_t idx) const override;
    void sortBy(YuGiOhBandaiSortColumn column, bool ascending) override;
    [[nodiscard]] bool matchesFilter(const YuGiOhBandaiCard& card,
                                     std::string_view filter) const override;
};

}  // namespace ccm::ui
