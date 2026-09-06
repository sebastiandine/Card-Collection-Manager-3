#include "ccm/services/MagicDeckCheck.hpp"

#include "ccm/util/AsciiUtils.hpp"

#include <cctype>
#include <unordered_map>
#include <utility>

namespace ccm {
namespace {

[[nodiscard]] bool isAsciiSpace(char c) noexcept {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

[[nodiscard]] bool isAsciiDigit(char c) noexcept {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

[[nodiscard]] bool isAsciiAlnum(char c) noexcept {
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

[[nodiscard]] std::string_view trimView(std::string_view s) {
    std::size_t begin = 0;
    while (begin < s.size() && isAsciiSpace(s[begin])) ++begin;
    std::size_t end = s.size();
    while (end > begin && isAsciiSpace(s[end - 1])) --end;
    return s.substr(begin, end - begin);
}

[[nodiscard]] bool isSectionHeader(std::string_view trimmed) {
    std::string lower = asciiLower(trimmed);
    if (!lower.empty() && lower.back() == ':') {
        lower.pop_back();
        while (!lower.empty() && isAsciiSpace(lower.back())) lower.pop_back();
    }
    return lower == "deck" || lower == "sideboard" || lower == "commander" ||
           lower == "companion" || lower == "maybeboard";
}

[[nodiscard]] bool isCollectorNumber(std::string_view token) {
    if (token.empty()) return false;
    std::size_t i = 0;
    while (i < token.size() && isAsciiDigit(token[i])) ++i;
    if (i == 0) return false;
    if (i == token.size()) return true;
    // Arena sometimes uses a letter suffix (`12a`).
    return i + 1 == token.size() &&
           std::isalpha(static_cast<unsigned char>(token[i])) != 0;
}

[[nodiscard]] bool isSetCode(std::string_view token) {
    if (token.size() < 2 || token.size() > 6) return false;
    for (char c : token) {
        if (!isAsciiAlnum(c)) return false;
    }
    return true;
}

// Strip a trailing ` (SET) 123` token pair (Arena / Moxfield).
[[nodiscard]] std::string stripArenaSuffix(std::string name) {
    const auto lastSpace = name.rfind(' ');
    if (lastSpace == std::string::npos) return name;

    const std::string_view last = std::string_view(name).substr(lastSpace + 1);
    if (!isCollectorNumber(last)) return name;

    std::string rest = name.substr(0, lastSpace);
    while (!rest.empty() && isAsciiSpace(rest.back())) rest.pop_back();
    if (rest.size() < 4 || rest.back() != ')') return name;

    const auto open = rest.rfind('(');
    if (open == std::string::npos) return name;
    const std::string_view setCode =
        std::string_view(rest).substr(open + 1, rest.size() - open - 2);
    if (!isSetCode(setCode)) return name;
    if (open > 0 && rest[open - 1] != ' ') return name;

    std::string out = rest.substr(0, open);
    while (!out.empty() && isAsciiSpace(out.back())) out.pop_back();
    return out;
}

[[nodiscard]] std::string_view stripSbPrefix(std::string_view line) {
    if (line.size() < 3) return line;
    const std::string head = asciiLower(line.substr(0, 3));
    if (head != "sb:") return line;
    std::size_t i = 3;
    while (i < line.size() && isAsciiSpace(line[i])) ++i;
    return line.substr(i);
}

[[nodiscard]] bool parseQuantityPrefix(std::string_view line,
                                       int&             quantity,
                                       std::string_view& rest) {
    std::size_t i = 0;
    while (i < line.size() && isAsciiSpace(line[i])) ++i;
    if (i >= line.size() || !isAsciiDigit(line[i])) return false;

    int value = 0;
    while (i < line.size() && isAsciiDigit(line[i])) {
        const int digit = line[i] - '0';
        if (value > (99999 - digit) / 10) return false;
        value = value * 10 + digit;
        ++i;
    }
    if (value <= 0) return false;

    if (i < line.size() && (line[i] == 'x' || line[i] == 'X')) ++i;
    if (i >= line.size() || !isAsciiSpace(line[i])) return false;
    while (i < line.size() && isAsciiSpace(line[i])) ++i;
    if (i >= line.size()) return false;

    quantity = value;
    rest = line.substr(i);
    return true;
}

}  // namespace

std::string normalizeMagicDeckName(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    bool pendingSpace = false;
    for (char c : name) {
        if (isAsciiSpace(c)) {
            if (!out.empty()) pendingSpace = true;
            continue;
        }
        if (pendingSpace) {
            out.push_back(' ');
            pendingSpace = false;
        }
        out.push_back(c);
    }
    return out;
}

std::vector<MagicDeckListEntry> parseMagicDeckList(std::string_view text) {
    std::vector<MagicDeckListEntry> out;
    std::unordered_map<std::string, std::size_t> indexByKey;

    std::size_t pos = 0;
    while (pos <= text.size()) {
        const auto nl = text.find('\n', pos);
        const auto end = nl == std::string_view::npos ? text.size() : nl;
        std::string_view raw = text.substr(pos, end - pos);
        if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);

        pos = nl == std::string_view::npos ? text.size() + 1 : nl + 1;

        std::string_view line = trimView(raw);
        if (line.empty()) continue;
        if (line.starts_with("//") || line.starts_with("#")) continue;
        if (isSectionHeader(line)) continue;

        line = stripSbPrefix(line);
        line = trimView(line);
        if (line.empty()) continue;

        int quantity = 0;
        std::string_view nameRest;
        if (!parseQuantityPrefix(line, quantity, nameRest)) continue;

        std::string name = normalizeMagicDeckName(stripArenaSuffix(
            normalizeMagicDeckName(nameRest)));
        if (name.empty()) continue;

        const std::string key = asciiLower(name);
        if (auto it = indexByKey.find(key); it != indexByKey.end()) {
            out[it->second].required += quantity;
            continue;
        }
        indexByKey.emplace(key, out.size());
        out.push_back(MagicDeckListEntry{std::move(name), quantity});
    }
    return out;
}

MagicDeckCheckResult
analyzeMagicDeckCheck(const std::vector<MagicDeckListEntry>& deck,
                      const std::vector<MagicCard>&          collection) {
    std::unordered_map<std::string, std::vector<const MagicCard*>> byName;
    byName.reserve(collection.size());
    for (const auto& card : collection) {
        const std::string key = asciiLower(normalizeMagicDeckName(card.name));
        if (key.empty()) continue;
        byName[key].push_back(&card);
    }

    MagicDeckCheckResult result;
    result.missing.reserve(deck.size());
    result.owned.reserve(deck.size());

    for (const auto& entry : deck) {
        if (entry.required <= 0 || entry.name.empty()) continue;
        const std::string key = asciiLower(normalizeMagicDeckName(entry.name));
        int ownedAmount = 0;
        std::vector<MagicCard> variations;
        if (auto it = byName.find(key); it != byName.end()) {
            variations.reserve(it->second.size());
            for (const MagicCard* card : it->second) {
                ownedAmount += static_cast<int>(card->amount);
                variations.push_back(*card);
            }
        }

        const int missingAmount =
            entry.required > ownedAmount ? entry.required - ownedAmount : 0;
        if (missingAmount > 0) {
            result.missing.push_back(
                MagicDeckMissingRow{entry.name, entry.required, missingAmount});
        }
        if (ownedAmount > 0) {
            result.owned.push_back(MagicDeckOwnedRow{
                entry.name, entry.required, ownedAmount, std::move(variations)});
        }
    }
    return result;
}

}  // namespace ccm
