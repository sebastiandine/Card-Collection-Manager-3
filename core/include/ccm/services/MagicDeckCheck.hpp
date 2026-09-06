#pragma once

// Pure helpers: parse a pasted Magic deck list and compare required copies
// against the collection by card name. Every printing / variation of a name
// counts toward the owned total (set, foil, language, and condition do not
// restrict the match).

#include "ccm/domain/MagicCard.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ccm {

struct MagicDeckListEntry {
    std::string name;
    int         required{0};
};

struct MagicDeckMissingRow {
    std::string name;
    int         required{0};
    int         missingAmount{0};
};

struct MagicDeckOwnedRow {
    std::string              name;
    int                      required{0};
    int                      ownedAmount{0};
    std::vector<MagicCard>   variations;
};

struct MagicDeckCheckResult {
    std::vector<MagicDeckMissingRow> missing;
    std::vector<MagicDeckOwnedRow>   owned;
};

// Collapse ASCII whitespace and trim. Used as the display-stable form of a
// pasted or collection name before case-fold matching.
[[nodiscard]] std::string normalizeMagicDeckName(std::string_view name);

// Parse numbered deck-list lines (`4 Lightning Bolt`, `4x Lightning Bolt`).
// Duplicate names merge (first-seen display name, summed quantity). Blank
// lines, `//` / `#` comments, and section headers are skipped. Arena/Moxfield
// trailing ` (SET) 123` is stripped from the name.
[[nodiscard]] std::vector<MagicDeckListEntry> parseMagicDeckList(std::string_view text);

// Compare a parsed list to the collection. Need 4 / own 2 → both lists
// (missing remaining 2, owned 2). Need 4 / own 0 → missing only. Need 4 /
// own 7 → owned only.
[[nodiscard]] MagicDeckCheckResult
analyzeMagicDeckCheck(const std::vector<MagicDeckListEntry>& deck,
                      const std::vector<MagicCard>&          collection);

}  // namespace ccm
