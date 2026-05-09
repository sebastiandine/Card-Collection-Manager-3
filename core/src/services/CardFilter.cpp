#include "ccm/services/CardFilter.hpp"

#include "ccm/domain/Enums.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace ccm {
namespace {

// Plain ASCII tolower, same approach as CardSorter::asciiLower. The old JS path used
// String.prototype.toLowerCase() which on the realistic ASCII-only data set
// (English/German set names, Scryfall-fed labels, integer amounts) behaves
// identically.
std::string asciiLower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        out.push_back(static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

bool containsLower(std::string_view haystack, std::string_view needleLower) {
    return asciiLower(haystack).find(needleLower) != std::string::npos;
}

}  // namespace

bool matchesMagicFilter(const MagicCard& card, std::string_view filter) {
    // `""`.includes(filter) is true for filter == "" in JS; mirror that so the
    // panel does not need a separate "no filter" branch.
    if (filter.empty()) return true;

    const std::string needle = asciiLower(filter);

    // Order mirrors the MtgTable.tsx tableFields list (minus the boolean
    // flag columns, which `applyFilter` skips). Stops on first match for the
    // same short-circuit behavior as the JS for-loop with `break`.
    if (containsLower(card.name, needle))                     return true;
    if (containsLower(card.set.name, needle))                 return true;
    if (containsLower(to_string(card.language), needle))      return true;
    if (containsLower(to_string(card.condition), needle))     return true;
    if (containsLower(std::to_string(card.amount), needle))   return true;
    if (containsLower(card.note, needle))                     return true;
    return false;
}

bool matchesPokemonFilter(const PokemonCard& card, std::string_view filter) {
    if (filter.empty()) return true;

    const std::string needle = asciiLower(filter);

    if (containsLower(card.name, needle))                     return true;
    if (containsLower(card.set.name, needle))                 return true;
    if (containsLower(card.setNo, needle))                    return true;
    if (containsLower(to_string(card.language), needle))      return true;
    if (containsLower(to_string(card.condition), needle))     return true;
    if (containsLower(std::to_string(card.amount), needle))   return true;
    if (containsLower(card.note, needle))                     return true;
    return false;
}

}  // namespace ccm
