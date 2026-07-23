#pragma once

// Pure helpers: Pokemon set-completion progress and per-set checklists.
// Ownership requires matching PokemonRegion for the pack (West vs Asia),
// matching set.id, and a normalized collector number / localId. Duplicates /
// amount / holo / firstEdition do not inflate the numerator. Optional
// regionFilter and languageFilter restrict which cards count (packs with
// zero matches are omitted).

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/PokemonCard.hpp"
#include "ccm/domain/PokemonSetCatalog.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ccm {

struct PokemonSetCompletionProgress {
    PokemonRegion region{PokemonRegion::West};
    std::string   setId;
    std::string   setName;
    std::size_t   ownedUnique{0};
    std::size_t   total{0};

    [[nodiscard]] int percent() const noexcept {
        if (total == 0) return 0;
        return static_cast<int>((ownedUnique * 100) / total);
    }
};

struct PokemonChecklistEntry {
    std::string setNo;
    std::string name;
    bool        owned{false};
};

// Distinct languages present in the collection (optionally region-scoped),
// in allLanguages() order.
[[nodiscard]] std::vector<Language>
pokemonLanguagesInCollection(const std::vector<PokemonCard>& collection,
                             std::optional<PokemonRegion> regionFilter = std::nullopt);

// Distinct regions that have ≥1 owned card matching a catalog pack.
[[nodiscard]] std::vector<PokemonRegion>
pokemonRegionsInCollection(const std::vector<PokemonCard>& collection,
                           const PokemonSetCatalog&        westCatalog,
                           const PokemonSetCatalog&        asiaCatalog);

// Packs where the collection owns ≥1 matching card, ordered by setName then
// region. When regionFilter is set, only that region's catalog/cards count.
[[nodiscard]] std::vector<PokemonSetCompletionProgress>
computePokemonSetCompletion(const std::vector<PokemonCard>& collection,
                            const PokemonSetCatalog&        westCatalog,
                            const PokemonSetCatalog&        asiaCatalog,
                            std::optional<PokemonRegion> regionFilter = std::nullopt,
                            std::optional<Language>      languageFilter = std::nullopt);

// Full catalog checklist for one pack; owned flags from the collection.
[[nodiscard]] std::vector<PokemonChecklistEntry>
pokemonChecklistForSet(const std::vector<PokemonCard>& collection,
                       const PokemonSetCatalog&        westCatalog,
                       const PokemonSetCatalog&        asiaCatalog,
                       PokemonRegion                   region,
                       std::string_view                setId,
                       std::optional<Language> languageFilter = std::nullopt);

}  // namespace ccm
