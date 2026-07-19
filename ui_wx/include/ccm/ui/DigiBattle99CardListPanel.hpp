#pragma once

#include "ccm/domain/DigiBattle99Card.hpp"
#include "ccm/services/CardSorter.hpp"
#include "ccm/ui/BaseCardListPanel.hpp"

namespace ccm::ui {

class DigiBattle99CardListPanel final
    : public BaseCardListPanel<DigiBattle99Card, DigiBattle99SortColumn> {
public:
    explicit DigiBattle99CardListPanel(wxWindow* parent);

protected:
    [[nodiscard]] std::vector<TextColumnSpec> declareTextColumns() const override;
    [[nodiscard]] std::vector<IconColumnSpec> declareIconColumns() const override;
    [[nodiscard]] std::string renderTextCell(const DigiBattle99Card& card,
                                             std::size_t idx) const override;
    [[nodiscard]] bool isIconColumnSet(const DigiBattle99Card& card,
                                       std::size_t idx) const override;
    void sortBy(DigiBattle99SortColumn column, bool ascending) override;
    [[nodiscard]] bool matchesFilter(const DigiBattle99Card& card,
                                     std::string_view filter) const override;
};

}  // namespace ccm::ui
