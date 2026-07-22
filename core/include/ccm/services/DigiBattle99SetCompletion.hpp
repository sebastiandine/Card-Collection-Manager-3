#pragma once

// Pure helpers: Digi-Battle set-completion progress and per-set checklists.
// Ownership counts only when collection card.set.id matches the pack and the
// normalized setNo appears in that pack's catalog. Duplicates / amount do not
// inflate the numerator.

#include "ccm/domain/DigiBattle99Card.hpp"
#include "ccm/domain/DigiBattle99SetCatalog.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ccm {

struct DigiBattle99SetCompletionProgress {
    std::string setId;
    std::string setName;
    std::size_t ownedUnique{0};
    std::size_t total{0};

    [[nodiscard]] int percent() const noexcept {
        if (total == 0) return 0;
        return static_cast<int>((ownedUnique * 100) / total);
    }
};

struct DigiBattle99ChecklistEntry {
    std::string setNo;
    std::string name;
    bool        owned{false};
};

// Packs where the collection owns ≥1 card with matching set.id, ordered by
// setName. Packs absent from the catalog are skipped.
[[nodiscard]] std::vector<DigiBattle99SetCompletionProgress>
computeDigiBattle99SetCompletion(const std::vector<DigiBattle99Card>& collection,
                                 const DigiBattle99SetCatalog&        catalog);

// Full catalog checklist for one pack; owned flags from the collection.
[[nodiscard]] std::vector<DigiBattle99ChecklistEntry>
digiBattle99ChecklistForSet(const std::vector<DigiBattle99Card>& collection,
                            const DigiBattle99SetCatalog&        catalog,
                            std::string_view                     setId);

}  // namespace ccm
