#pragma once

// PokemonCardListPanel: typed view of the Pokemon collection. Inherits all
// `wxListCtrl`/themed-header machinery from `BaseCardListPanel<PokemonCard,
// PokemonSortColumn>` and only overrides the per-game hooks.

#include "ccm/domain/PokemonCard.hpp"
#include "ccm/services/CardSorter.hpp"
#include "ccm/ui/BaseCardListPanel.hpp"

namespace ccm::ui {

class PokemonCardListPanel final : public BaseCardListPanel<PokemonCard, PokemonSortColumn> {
public:
    explicit PokemonCardListPanel(wxWindow* parent);

protected:
    [[nodiscard]] std::vector<TextColumnSpec> declareTextColumns() const override;
    [[nodiscard]] std::vector<IconColumnSpec> declareIconColumns() const override;
    [[nodiscard]] std::string renderTextCell(const PokemonCard& card, std::size_t idx) const override;
    [[nodiscard]] bool isIconColumnSet(const PokemonCard& card, std::size_t idx) const override;
    void sortBy(PokemonSortColumn column, bool ascending) override;
    [[nodiscard]] bool matchesFilter(const PokemonCard& card, std::string_view filter) const override;
};

}  // namespace ccm::ui
