#pragma once

// Resolves a Yu-Gi-Oh! product code (YGOPRODeck `set_code`, stored as `Set.id`)
// against a cached set list. Used by the Yu-Gi-Oh! edit dialog "set code" mode.

#include "ccm/domain/Set.hpp"

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ccm {

struct YuGiOhSetShorthandLookup {
    enum class Kind { Unique, NotFound, Ambiguous };

    Kind kind{Kind::NotFound};
    std::size_t index{0};
};

[[nodiscard]] inline std::string normalizeYuGiOhSetIdForLookup(std::string_view id) {
    std::string out;
    out.reserve(id.size());
    for (unsigned char uch : id) {
        out.push_back(static_cast<char>(std::tolower(uch)));
    }
    return out;
}

[[nodiscard]] inline std::string_view trimAsciiWhitespace(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

[[nodiscard]] inline YuGiOhSetShorthandLookup lookupYuGiOhSetByShorthand(
    std::string_view query, const std::vector<Set>& sets) {
    const std::string_view trimmed = trimAsciiWhitespace(query);
    if (trimmed.empty()) {
        return {YuGiOhSetShorthandLookup::Kind::NotFound, 0};
    }
    const std::string qNorm = normalizeYuGiOhSetIdForLookup(trimmed);

    std::size_t firstIdx = 0;
    int matchCount = 0;
    for (std::size_t i = 0; i < sets.size(); ++i) {
        if (normalizeYuGiOhSetIdForLookup(sets[i].id) == qNorm) {
            if (matchCount == 0) firstIdx = i;
            ++matchCount;
            if (matchCount > 1) {
                return {YuGiOhSetShorthandLookup::Kind::Ambiguous, 0};
            }
        }
    }
    if (matchCount == 1) {
        return {YuGiOhSetShorthandLookup::Kind::Unique, firstIdx};
    }
    return {YuGiOhSetShorthandLookup::Kind::NotFound, 0};
}

}  // namespace ccm
