#pragma once

// JapanesePokemonCardListPanel: typed view of the Pokemon collection. Inherits all
// `wxListCtrl`/themed-header machinery from `BaseCardListPanel<JapanesePokemonCard,
// JapanesePokemonSortColumn>` and only overrides the per-game hooks.

#include "ccm/domain/JapanesePokemonCard.hpp"
#include "ccm/services/CardSorter.hpp"
#include "ccm/ui/BaseCardListPanel.hpp"

namespace ccm::ui {

class JapanesePokemonCardListPanel final : public BaseCardListPanel<JapanesePokemonCard, JapanesePokemonSortColumn> {
public:
    explicit JapanesePokemonCardListPanel(wxWindow* parent);

protected:
    [[nodiscard]] std::vector<TextColumnSpec> declareTextColumns() const override;
    [[nodiscard]] std::vector<IconColumnSpec> declareIconColumns() const override;
    [[nodiscard]] std::string renderTextCell(const JapanesePokemonCard& card, std::size_t idx) const override;
    [[nodiscard]] bool isIconColumnSet(const JapanesePokemonCard& card, std::size_t idx) const override;
    void sortBy(JapanesePokemonSortColumn column, bool ascending) override;
    [[nodiscard]] bool matchesFilter(const JapanesePokemonCard& card, std::string_view filter) const override;
};

}  // namespace ccm::ui
