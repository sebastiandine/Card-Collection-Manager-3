#pragma once

// Pure helpers: Bandai set-completion progress and per-set checklists.
// Ownership keys on (set.id, normalized setNo). Never name-only.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/YuGiOhBandaiCard.hpp"
#include "ccm/domain/YuGiOhBandaiSetCatalog.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ccm {

struct YuGiOhBandaiSetCompletionProgress {
    std::string setId;
    std::string setName;
    std::size_t ownedUnique{0};
    std::size_t total{0};

    [[nodiscard]] int percent() const noexcept {
        if (total == 0) return 0;
        return static_cast<int>((ownedUnique * 100) / total);
    }
};

struct YuGiOhBandaiChecklistEntry {
    std::string setNo;
    std::string name;
    std::string rarity;
    bool        owned{false};
};

[[nodiscard]] std::vector<Language>
yuGiOhBandaiLanguagesInCollection(const std::vector<YuGiOhBandaiCard>& collection);

[[nodiscard]] std::vector<YuGiOhBandaiSetCompletionProgress>
computeYuGiOhBandaiSetCompletion(const std::vector<YuGiOhBandaiCard>& collection,
                                 const YuGiOhBandaiSetCatalog&        catalog,
                                 std::optional<Language> languageFilter = std::nullopt);

[[nodiscard]] std::vector<YuGiOhBandaiChecklistEntry>
yuGiOhBandaiChecklistForSet(const std::vector<YuGiOhBandaiCard>& collection,
                            const YuGiOhBandaiSetCatalog&        catalog,
                            std::string_view                     setId,
                            std::optional<Language> languageFilter = std::nullopt);

}  // namespace ccm
