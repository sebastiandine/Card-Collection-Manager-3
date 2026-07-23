#pragma once

// Canonicalize legacy pokemontcg.io West set ids to TCGdex EN ids.
// Identity when the id is already TCGdex (or unknown). Asia set ids must not
// be passed through this helper.

#include <string>
#include <string_view>

namespace ccm {

// Returns the TCGdex EN set id for a West Pokemon card.set.id. Unknown ids
// and ids that already match TCGdex are returned unchanged.
[[nodiscard]] std::string canonicalizeWestSetId(std::string_view setId);

}  // namespace ccm
