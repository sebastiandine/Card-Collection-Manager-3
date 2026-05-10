#pragma once

// Yu-Gi-Oh! collector slot equivalence for UI + metadata matching.
//
// The edit dialog composes `setNo` as `<set.id>-<digits>` using only numeric
// characters from the text field (e.g. SOD + "015" -> "SOD-015"). YGOPRODeck
// `set_code` values often embed region letters ("SOD-EN015"). Exact string
// compare would miss that both refer to the same slot.

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace ccm {

[[nodiscard]] inline std::string_view trimAsciiSpaces(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.remove_suffix(1);
    }
    return s;
}

[[nodiscard]] inline std::string ygoAbbrevBeforeDash(std::string_view raw) {
    const std::string_view s = trimAsciiSpaces(raw);
    const auto dash = s.find('-');
    const std::string_view pref = dash == std::string_view::npos ? s : s.substr(0, dash);
    std::string out(pref);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

[[nodiscard]] inline std::string ygoCollectorDigitsOnly(std::string_view raw) {
    const std::string_view s = trimAsciiSpaces(raw);
    const auto dash = s.find('-');
    const std::string_view tail =
        dash == std::string_view::npos ? std::string_view{} : s.substr(dash + 1);
    std::string out;
    out.reserve(tail.size());
    for (unsigned char c : tail) {
        if (std::isdigit(c) != 0) out.push_back(static_cast<char>(c));
    }
    return out;
}

// True when both strings designate the same printed slot: same abbreviation
// before the first '-' (ASCII case-insensitive) and the same ordered digit run
// extracted from everything after that dash.
[[nodiscard]] inline bool ygoPrintingSlotsMatch(std::string_view a, std::string_view b) {
    if (ygoAbbrevBeforeDash(a) != ygoAbbrevBeforeDash(b)) return false;
    return ygoCollectorDigitsOnly(a) == ygoCollectorDigitsOnly(b);
}

// YGOPRODeck sometimes lists European alternate numbering alongside NA prints under
// the same English `set_name` (e.g. Dark Magician as "LOB-E003" vs NA "LOB-005").
// The suffix uses a single leading `E` immediately followed by digits — distinct
// from two-letter regions such as "EN" ("LOB-EN005") or "DE" ("LOB-DE005").
[[nodiscard]] inline bool ygoLikelyEuropeanRegionalSetCode(std::string_view setCode) {
    const std::string_view s = trimAsciiSpaces(setCode);
    const auto dash = s.find('-');
    if (dash == std::string_view::npos || dash + 2 >= s.size()) return false;
    const std::string_view tail = s.substr(dash + 1);
    return tail.size() >= 2 && tail[0] == 'E'
        && std::isdigit(static_cast<unsigned char>(tail[1])) != 0;
}

// Canonical short-form for Yu-Gi-Oh rarities used by the overview table.
// Returns empty when rarity is unknown.
[[nodiscard]] inline std::string ygoRarityShortCode(std::string_view rarity) {
    std::string normalized;
    normalized.reserve(rarity.size());
    for (unsigned char c : rarity) {
        if (std::isspace(c) != 0) continue;
        if (c == '\'' || c == '`' || c == '-') continue;
        normalized.push_back(static_cast<char>(std::tolower(c)));
    }

    if (normalized == "common") return "C";
    if (normalized == "rare") return "R";
    if (normalized == "superrare") return "SR";
    if (normalized == "ultrarare") return "UR";
    if (normalized == "secretrare") return "ScR";
    if (normalized == "quartercenturysecretrare") return "QCScR";
    if (normalized == "qcsr") return "QCScR";
    if (normalized == "starlightrare") return "StR";
    if (normalized == "collectorsrare") return "CR";
    if (normalized == "ghostrare") return "GR";
    if (normalized == "ultimaterare") return "UtR";
    if (normalized == "platinumsecretrare") return "PlScR";
    if (normalized == "prismaticsecretrare") return "PScR";
    return {};
}

}  // namespace ccm
