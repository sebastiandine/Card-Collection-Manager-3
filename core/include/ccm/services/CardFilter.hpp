#pragma once

// CardFilter - row matcher aligned with TableTemplate.tsx::applyFilter semantics.
// kept a single `filter` string in the table component and, before rendering,
// kept only the rows where *any* of the `tableFields` value-key columns
// (string- or number-typed) contained the filter as a substring. Boolean flag
// columns (foil / signed / altered / holo / firstEdition) were skipped because
// `typeof` was neither "string" nor "number".
//
// We replicate that logic here as free functions so the UI can stay dumb (it
// just owns a wxTextCtrl and re-asks core which rows match).
//
// Differences worth knowing:
//   * The match is case-insensitive on **both** sides — the old JS path only lowercased
//     the cell value, leaving the filter as-typed, which made uppercase input
//     never match. Lowercasing the filter too is the obvious-intent fix and
//     keeps round-trip semantics for any filter the original UI would accept
//     (lowercase filters behave identically).
//   * An empty filter matches every row, exactly as in JS where every string
//     `.includes("")` returns true.

#include "ccm/domain/MagicCard.hpp"
#include "ccm/domain/PokemonCard.hpp"

#include <string_view>

namespace ccm {

// Magic value-key columns from the MtgTable.tsx tableFields list:
//   name, set.name, language, condition, amount, note.
// Foil/Signed/Altered are bool-typed and intentionally excluded.
[[nodiscard]] bool matchesMagicFilter(const MagicCard& card,
                                      std::string_view filter);

// Pokemon value-key columns from PokemonTable.tsx tableFields list:
//   name, set.name, setNo, language, condition, amount, note.
// Holo/FirstEdition/Signed/Altered are bool-typed and excluded.
[[nodiscard]] bool matchesPokemonFilter(const PokemonCard& card,
                                        std::string_view filter);

}  // namespace ccm
