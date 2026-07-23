#pragma once

// Pure helpers: Digi-Battle set-completion progress and per-set checklists.
// Ownership counts only when collection card.set.id matches the pack and the
// normalized setNo appears in that pack's catalog. Duplicates / amount do not
// inflate the numerator. An optional languageFilter restricts ownership to
// cards of that language (packs with zero matches are omitted).

#include "ccm/domain/DigiBattle99Card.hpp"
#include "ccm/domain/DigiBattle99SetCatalog.hpp"
#include "ccm/domain/Enums.hpp"

#include <cstddef>
#include <optional>
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

// Distinct languages present in the collection, in allLanguages() order.
[[nodiscard]] std::vector<Language>
digiBattle99LanguagesInCollection(const std::vector<DigiBattle99Card>& collection);

// Packs where the collection owns ≥1 card with matching set.id, ordered by
// setName. Packs absent from the catalog are skipped. When languageFilter is
// set, only cards of that language count toward ownership.
[[nodiscard]] std::vector<DigiBattle99SetCompletionProgress>
computeDigiBattle99SetCompletion(const std::vector<DigiBattle99Card>& collection,
                                 const DigiBattle99SetCatalog&        catalog,
                                 std::optional<Language> languageFilter = std::nullopt);

// Full catalog checklist for one pack; owned flags from the collection.
// When languageFilter is set, only cards of that language count as owned.
[[nodiscard]] std::vector<DigiBattle99ChecklistEntry>
digiBattle99ChecklistForSet(const std::vector<DigiBattle99Card>& collection,
                            const DigiBattle99SetCatalog&        catalog,
                            std::string_view                     setId,
                            std::optional<Language> languageFilter = std::nullopt);

}  // namespace ccm
