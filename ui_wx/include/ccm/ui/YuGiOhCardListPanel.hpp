#pragma once

#include "ccm/domain/YuGiOhCard.hpp"
#include "ccm/services/CardSorter.hpp"
#include "ccm/ui/BaseCardListPanel.hpp"

namespace ccm::ui {

class YuGiOhCardListPanel final : public BaseCardListPanel<YuGiOhCard, YuGiOhSortColumn> {
public:
    explicit YuGiOhCardListPanel(wxWindow* parent);

protected:
    [[nodiscard]] std::vector<TextColumnSpec> declareTextColumns() const override;
    [[nodiscard]] std::vector<IconColumnSpec> declareIconColumns() const override;
    [[nodiscard]] std::string renderTextCell(const YuGiOhCard& card, std::size_t idx) const override;
    [[nodiscard]] bool isIconColumnSet(const YuGiOhCard& card, std::size_t idx) const override;
    void sortBy(YuGiOhSortColumn column, bool ascending) override;
    [[nodiscard]] bool matchesFilter(const YuGiOhCard& card, std::string_view filter) const override;
};

}  // namespace ccm::ui
