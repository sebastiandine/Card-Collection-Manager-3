#pragma once

// CardSorter - per-column sorting behavior matching the established table model
// `tableFields` configuration. The original TS code stored each column's
// (valueKey, sortKey) pair on a config object so that, e.g., the "Set" column
// could *display* `set.name` while sorting by `set.releaseDate`. We replicate
// that mapping here as two enums (one per supported game), each variant naming
// a sort key. The free functions below run a stable sort over a vector using
// the same per-type rules as the original `byField`:
//
//   * strings  -> case-insensitive (lowercased) `<` / `>`
//   * numbers  -> direct `<` / `>`
//   * booleans -> direct `<` / `>` (so false < true; unset flags first when asc)
//
// Stable sort matches the JS `Array.prototype.sort` guarantee from ES2019; the
// UI relies on it so successive clicks on different columns compose predictably
// (e.g. sort by name, then by set => grouped by set, name-sorted within each).

#include "ccm/domain/MagicCard.hpp"
#include "ccm/domain/PokemonCard.hpp"
#include "ccm/domain/YuGiOhCard.hpp"

#include <vector>

namespace ccm {

// One value per *real* (non-spacer) column of the Magic table, in the same
// order as the Magic tableFields list.
enum class MagicSortColumn {
    Name,
    SetReleaseDate,  // value column shows set.name, sort key is set.releaseDate
    Language,
    Condition,
    Amount,
    Foil,
    Signed,
    Altered,
    Note,
};

// Pokemon equivalent (PokemonTable.tsx tableFields). Adds Holo + FirstEdition,
// drops Foil.
enum class PokemonSortColumn {
    Name,
    SetReleaseDate,
    Language,
    Condition,
    Amount,
    Holo,
    FirstEdition,
    Signed,
    Altered,
    Note,
};

enum class YuGiOhSortColumn {
    Name,
    SetReleaseDate,
    Language,
    Condition,
    Amount,
    FirstEdition,
    Signed,
    Altered,
    Note,
};

// Stable in-place sort. `ascending=false` runs the same comparator with
// inverted sign, matching `byField(field, asc)` semantics.
void sortMagicCards(std::vector<MagicCard>& cards, MagicSortColumn column,
                    bool ascending);
void sortPokemonCards(std::vector<PokemonCard>& cards, PokemonSortColumn column,
                      bool ascending);
void sortYuGiOhCards(std::vector<YuGiOhCard>& cards, YuGiOhSortColumn column,
                     bool ascending);

}  // namespace ccm
