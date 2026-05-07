#pragma once

// MagicCardListPanel: typed view of the Magic collection. Inherits all
// `wxListCtrl`/themed-header machinery from `BaseCardListPanel<MagicCard,
// MagicSortColumn>`; this header only declares the per-game hook overrides
// (column layout, sort/filter dispatch, cell rendering).
//
// The legacy `EVT_MAGIC_CARD_SELECTED` alias is kept as a deprecated typedef
// so any out-of-tree callers keep building; new code should bind the shared
// `EVT_CARD_SELECTED` event from `BaseCardListPanel.hpp`.

#include "ccm/domain/MagicCard.hpp"
#include "ccm/services/CardSorter.hpp"
#include "ccm/ui/BaseCardListPanel.hpp"

namespace ccm::ui {

// Backwards-compatible alias for callers that bound the old event symbol.
inline const auto& EVT_MAGIC_CARD_SELECTED = EVT_CARD_SELECTED;

class MagicCardListPanel final : public BaseCardListPanel<MagicCard, MagicSortColumn> {
public:
    explicit MagicCardListPanel(wxWindow* parent);

protected:
    [[nodiscard]] std::vector<TextColumnSpec> declareTextColumns() const override;
    [[nodiscard]] std::vector<IconColumnSpec> declareIconColumns() const override;
    [[nodiscard]] std::string renderTextCell(const MagicCard& card, std::size_t idx) const override;
    [[nodiscard]] bool isIconColumnSet(const MagicCard& card, std::size_t idx) const override;
    void sortBy(MagicSortColumn column, bool ascending) override;
    [[nodiscard]] bool matchesFilter(const MagicCard& card, std::string_view filter) const override;
};

}  // namespace ccm::ui
