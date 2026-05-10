#include "ccm/services/CardFilter.hpp"

#include "ccm/domain/Enums.hpp"
#include "ccm/util/AsciiUtils.hpp"

#include <string>
#include <string_view>

namespace ccm {
namespace {

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

bool matchesYuGiOhFilter(const YuGiOhCard& card, std::string_view filter) {
    if (filter.empty()) return true;

    const std::string needle = asciiLower(filter);

    if (containsLower(card.name, needle))                     return true;
    if (containsLower(card.set.name, needle))                 return true;
    if (containsLower(card.setNo, needle))                    return true;
    if (containsLower(card.rarity, needle))                   return true;
    if (containsLower(ygoRarityShortCode(card.rarity), needle)) return true;
    if (containsLower(to_string(card.language), needle))      return true;
    if (containsLower(to_string(card.condition), needle))     return true;
    if (containsLower(std::to_string(card.amount), needle))   return true;
    if (containsLower(card.note, needle))                     return true;
    return false;
}

}  // namespace ccm
