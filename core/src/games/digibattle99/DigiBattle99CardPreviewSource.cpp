#include "ccm/games/digibattle99/DigiBattle99CardPreviewSource.hpp"

#include "ccm/util/Rfc3986.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

namespace ccm {

namespace {

std::string trim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string toLower(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

bool cardInPack(const nlohmann::json& card, std::string_view packName) {
    if (packName.empty()) return true;
    if (!card.contains("set_name") || !card.at("set_name").is_array()) return false;
    for (const auto& pack : card.at("set_name")) {
        if (pack.is_string() && pack.get<std::string>() == packName) return true;
    }
    return false;
}

// Numeric collector suffix: "ST-01" → "01", "01" → "01", "BO-115" → "115".
std::string numericSuffix(std::string_view setNo) {
    const std::string n = DigiBattle99CardPreviewSource::normalizeCardNumber(setNo);
    const auto dash = n.find('-');
    const std::string_view tail =
        dash == std::string::npos ? std::string_view{n} : std::string_view{n}.substr(dash + 1);
    std::string out;
    out.reserve(tail.size());
    for (unsigned char c : tail) {
        if (std::isdigit(c) != 0) out.push_back(static_cast<char>(c));
    }
    return out;
}

std::string stripLeadingZeros(std::string digits) {
    std::size_t i = 0;
    while (i + 1 < digits.size() && digits[i] == '0') ++i;
    if (i > 0) digits.erase(0, i);
    return digits;
}

bool hasAlphabeticPrefix(std::string_view setNo) {
    const std::string n = DigiBattle99CardPreviewSource::normalizeCardNumber(setNo);
    return !n.empty() && std::isalpha(static_cast<unsigned char>(n.front())) != 0;
}

// Exact id match, or digits-only input matched to the numeric suffix with
// leading zeros ignored ("1" ↔ "ST-01", but not "ST-11").
bool cardNumbersMatch(std::string_view wanted, std::string_view actual) {
    const std::string a = DigiBattle99CardPreviewSource::normalizeCardNumber(wanted);
    const std::string b = DigiBattle99CardPreviewSource::normalizeCardNumber(actual);
    if (a.empty() || b.empty()) return false;
    if (a == b) return true;
    // Full id typed (ST-01): require exact normalized equality only.
    if (hasAlphabeticPrefix(a)) return false;
    return stripLeadingZeros(numericSuffix(a)) == stripLeadingZeros(numericSuffix(b));
}

}  // namespace

DigiBattle99CardPreviewSource::DigiBattle99CardPreviewSource(IHttpClient& http)
    : http_(http) {}

std::string DigiBattle99CardPreviewSource::normalizeCardNumber(std::string_view setNo) {
    std::string s = trim(std::string(setNo));
    if (s.empty()) return s;
    // Uppercase leading alphabetic prefix (ST / BO / MO / Fx-style).
    std::size_t i = 0;
    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i]))) {
        s[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[i])));
        ++i;
    }
    return s;
}

std::string DigiBattle99CardPreviewSource::buildImageUrl(std::string_view setNo) {
    const std::string id = normalizeCardNumber(setNo);
    return std::string(kImageBase) + id + ".jpg";
}

std::string DigiBattle99CardPreviewSource::buildSearchUrl(std::string_view name,
                                                          std::string_view setName,
                                                          std::string_view setNo) {
    std::string url = "https://digimoncard.io/api-public/search.php?series=";
    url += rfc3986PercentEncode(kSeries);
    if (!name.empty()) {
        url += "&n=";
        url += rfc3986PercentEncode(name);
    }
    if (!setName.empty()) {
        url += "&pack=";
        url += rfc3986PercentEncode(setName);
    }
    const std::string num = normalizeCardNumber(setNo);
    if (!num.empty()) {
        url += "&card=";
        url += rfc3986PercentEncode(num);
    }
    url += "&sort=name&sortdirection=asc";
    return url;
}

Result<std::string, PreviewLookupError>
DigiBattle99CardPreviewSource::parseImageUrlFromSearch(const std::string& body,
                                                       std::string_view wantedCardName) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (j.is_object() && j.contains("error")) {
            return R::err({K::NotFound, j.value("error", std::string{"No cards found."})});
        }
        if (!j.is_array()) {
            return R::err({K::Transient, "digimoncard.io Digi-Battle response is not a JSON array."});
        }
        if (j.empty()) {
            return R::err({K::NotFound, "digimoncard.io returned no matching Digi-Battle cards."});
        }

        const std::string wantedLower = toLower(trim(std::string(wantedCardName)));
        const nlohmann::json* chosen = nullptr;
        for (const auto& card : j) {
            if (!wantedLower.empty()) {
                const std::string cardName = trim(card.value("name", ""));
                if (toLower(cardName) != wantedLower) continue;
            }
            chosen = &card;
            break;
        }
        if (chosen == nullptr) {
            return R::err({K::NotFound, "digimoncard.io returned no matching Digi-Battle cards."});
        }
        const std::string id = normalizeCardNumber(chosen->value("id", ""));
        if (id.empty()) {
            return R::err({K::NotFound, "Digi-Battle card has no id / card number."});
        }
        return R::ok(buildImageUrl(id));
    } catch (const std::exception& e) {
        return R::err({K::Transient,
            std::string("digimoncard.io Digi-Battle JSON parse error: ") + e.what()});
    }
}

Result<std::string, PreviewLookupError>
DigiBattle99CardPreviewSource::fetchImageUrl(std::string_view name,
                                             std::string_view setName,
                                             std::string_view setNo) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;

    const std::string num = normalizeCardNumber(setNo);
    if (!num.empty()) {
        return R::ok(buildImageUrl(num));
    }
    if (name.empty()) {
        return R::err({K::NotFound, "Digi-Battle preview requires a card name or set number."});
    }

    const std::string url = buildSearchUrl(name, setName, "");
    auto resp = http_.get(url);
    if (!resp) return R::err({K::Transient, resp.error()});
    return parseImageUrlFromSearch(resp.value(), name);
}

Result<std::vector<AutoDetectedPrint>> DigiBattle99CardPreviewSource::parsePrintVariants(
    const std::string& body,
    std::string_view setName,
    std::string_view wantedCardName,
    std::string_view wantedSetNo) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    try {
        const auto j = nlohmann::json::parse(body);
        if (j.is_object() && j.contains("error")) {
            return R::err(j.value("error", std::string{"No cards found."}));
        }
        if (!j.is_array() || j.empty()) {
            return R::err("digimoncard.io returned no matching Digi-Battle cards.");
        }

        const std::string wantedPack = trim(std::string(setName));
        const std::string wantedNameLower = toLower(trim(std::string(wantedCardName)));
        const std::string wantedNo = normalizeCardNumber(wantedSetNo);

        std::vector<AutoDetectedPrint> collected;
        for (const auto& card : j) {
            if (!wantedNameLower.empty()) {
                const std::string cardName = trim(card.value("name", ""));
                if (toLower(cardName) != wantedNameLower) continue;
            }
            if (!cardInPack(card, wantedPack)) continue;
            AutoDetectedPrint out;
            out.name = trim(card.value("name", ""));
            out.setNo = normalizeCardNumber(card.value("id", ""));
            out.rarity = "";  // Digi-Battle UI is Pokémon-like; rarity not persisted.
            if (out.setNo.empty()) continue;
            // digimoncard.io `card=` is fuzzy (card=1 can return ST-01 and ST-11).
            // When the user typed a number, keep only exact / zero-padded matches.
            if (!wantedNo.empty() && !cardNumbersMatch(wantedNo, out.setNo)) continue;
            collected.push_back(std::move(out));
        }

        if (collected.empty()) {
            if (!wantedNo.empty()) {
                return R::err("Could not auto-detect Digi-Battle card name from set number.");
            }
            if (!wantedNameLower.empty() && !wantedPack.empty()) {
                return R::err("Could not auto-detect Digi-Battle set print metadata.");
            }
            return R::err("digimoncard.io returned no matching Digi-Battle cards.");
        }

        std::vector<AutoDetectedPrint> deduped;
        deduped.reserve(collected.size());
        std::unordered_set<std::string> seen;
        seen.reserve(collected.size() * 2);
        for (auto& p : collected) {
            if (seen.insert(p.setNo).second) deduped.push_back(std::move(p));
        }
        return R::ok(std::move(deduped));
    } catch (const std::exception& e) {
        return R::err(std::string("digimoncard.io Digi-Battle JSON parse error: ") + e.what());
    }
}

Result<AutoDetectedPrint> DigiBattle99CardPreviewSource::detectFirstPrint(
    std::string_view name,
    std::string_view setName) {
    auto list = detectPrintVariants(name, setName);
    if (!list || list.value().empty()) {
        if (!list) return Result<AutoDetectedPrint>::err(list.error());
        return Result<AutoDetectedPrint>::err("Could not auto-detect Digi-Battle set print metadata.");
    }
    return Result<AutoDetectedPrint>::ok(list.value().front());
}

Result<std::vector<AutoDetectedPrint>> DigiBattle99CardPreviewSource::detectPrintVariants(
    std::string_view name,
    std::string_view setName) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    const std::string url = buildSearchUrl(name, setName, "");
    auto resp = http_.get(url);
    if (resp) {
        return parsePrintVariants(resp.value(), setName, name);
    }
    // Retry name-only; still filter by pack in parsePrintVariants.
    const std::string fallbackUrl = buildSearchUrl(name, "", "");
    auto fallback = http_.get(fallbackUrl);
    if (!fallback) return R::err(fallback.error());
    return parsePrintVariants(fallback.value(), setName, name);
}

Result<AutoDetectedPrint> DigiBattle99CardPreviewSource::detectBySetNo(
    std::string_view setName,
    std::string_view setNo) {
    auto list = detectVariantsBySetNo(setName, setNo);
    if (!list) return Result<AutoDetectedPrint>::err(list.error());
    if (list.value().empty()) {
        return Result<AutoDetectedPrint>::err(
            "Could not auto-detect Digi-Battle card name from set number.");
    }
    return Result<AutoDetectedPrint>::ok(list.value().front());
}

Result<std::vector<AutoDetectedPrint>> DigiBattle99CardPreviewSource::detectVariantsBySetNo(
    std::string_view setName,
    std::string_view setNo) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    if (trim(std::string(setName)).empty()) return R::err("Select a set first.");
    const std::string num = normalizeCardNumber(setNo);
    if (num.empty()) return R::err("Card number is empty.");

    const std::string url = buildSearchUrl("", setName, num);
    auto resp = http_.get(url);
    if (resp) {
        auto parsed = parsePrintVariants(resp.value(), setName, "", num);
        if (parsed && !parsed.value().empty()) return parsed;
    }
    // Retry number-only; still filter by pack + exact/padded number.
    const std::string fallbackUrl = buildSearchUrl("", "", num);
    auto fallback = http_.get(fallbackUrl);
    if (!fallback) {
        if (resp) return R::err("Could not auto-detect Digi-Battle card name from set number.");
        return R::err(fallback.error());
    }
    return parsePrintVariants(fallback.value(), setName, "", num);
}

}  // namespace ccm
