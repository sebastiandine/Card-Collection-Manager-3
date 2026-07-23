#pragma once

// Sync Pokemon collection cards against freshly fetched set lists:
// - West: canonicalize legacy pokemontcg set ids, then refresh name/date
// - Asia: refresh name/date when the set id is present in the Asia list

#include "ccm/domain/PokemonCard.hpp"
#include "ccm/domain/Set.hpp"

#include <cstddef>
#include <vector>

namespace ccm {

// Mutates cards in place. Returns how many cards changed at least one set field.
[[nodiscard]] std::size_t syncPokemonCollectionSets(
    std::vector<PokemonCard>& cards,
    const std::vector<Set>&   westSets,
    const std::vector<Set>&   asiaSets);

}  // namespace ccm
