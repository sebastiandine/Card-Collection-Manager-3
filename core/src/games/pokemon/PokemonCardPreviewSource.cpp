#include "ccm/games/pokemon/PokemonCardPreviewSource.hpp"

#include "ccm/games/pokemon/PokemonWestSetId.hpp"
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

}  // namespace

PokemonCardPreviewSource::PokemonCardPreviewSource(IHttpClient& http) : http_(http) {}

std::string PokemonCardPreviewSource::normalizeCollectorNumber(std::string_view setNo) {
    std::string s(setNo);
    const auto slash = s.find('/');
    if (slash != std::string::npos) {
        s = s.substr(0, slash);
    }
    return s;
}

std::string PokemonCardPreviewSource::imageUrlFromBase(std::string_view imageBase) {
    if (imageBase.empty()) return {};
    std::string url(imageBase);
    while (!url.empty() && (url.back() == '/' || url.back() == ' ')) url.pop_back();
    return url + "/high.png";
}

std::string PokemonCardPreviewSource::buildCardByIdUrl(std::string_view setId,
                                                       std::string_view setNo) {
    const std::string idCanon = canonicalizeWestSetId(setId);
    const std::string num = normalizeCollectorNumber(setNo);
    std::string id = idCanon + "-" + num;
    return std::string("https://api.tcgdex.net/v2/en/cards/") + rfc3986PercentEncode(id);
}

std::string PokemonCardPreviewSource::buildSetDetailUrl(std::string_view setId) {
    return std::string("https://api.tcgdex.net/v2/en/sets/") +
           rfc3986PercentEncode(canonicalizeWestSetId(setId));
}

std::string PokemonCardPreviewSource::buildSearchUrl(std::string_view name,
                                                     std::string_view setId,
                                                     std::string_view setNo) {
    const std::string idCanon = canonicalizeWestSetId(setId);
    const std::string num = normalizeCollectorNumber(setNo);
    std::string url = "https://api.tcgdex.net/v2/en/cards?";
    bool first = true;
    auto append = [&](std::string_view key, std::string_view value) {
        if (value.empty()) return;
        if (!first) url += '&';
        first = false;
        url += std::string(key);
        url += "=eq:";
        url += rfc3986PercentEncode(value);
    };

    if (!idCanon.empty() && !num.empty()) {
        append("set.id", idCanon);
        append("localId", num);
    } else {
        append("name", name);
        append("set.id", idCanon);
        append("localId", num);
    }
    return url;
}

Result<std::vector<PokemonCardPreviewSource::SetCardRow>, PreviewLookupError>
PokemonCardPreviewSource::parseSetCards(const std::string& body) {
    using R = Result<std::vector<SetCardRow>, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.is_object() || !j.contains("cards") || !j.at("cards").is_array()) {
            return R::err({K::Transient,
                           "TCGdex EN set detail missing 'cards' array."});
        }
        std::vector<SetCardRow> out;
        out.reserve(j.at("cards").size());
        for (const auto& card : j.at("cards")) {
            SetCardRow row;
            row.localId = card.value("localId", "");
            if (row.localId.empty() && card.contains("id") && card.at("id").is_string()) {
                const std::string id = card.at("id").get<std::string>();
                const auto dash = id.rfind('-');
                if (dash != std::string::npos) row.localId = id.substr(dash + 1);
            }
            row.name = card.value("name", "");
            row.rarity = card.value("rarity", "");
            if (card.contains("image") && card.at("image").is_string()) {
                row.imageBase = card.at("image").get<std::string>();
            }
            if (row.localId.empty()) continue;
            out.push_back(std::move(row));
        }
        return R::ok(std::move(out));
    } catch (const std::exception& e) {
        return R::err({K::Transient,
                       std::string("TCGdex EN set detail JSON parse error: ") + e.what()});
    }
}

Result<std::string, PreviewLookupError>
PokemonCardPreviewSource::parseCardByIdResponse(const std::string& body) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.is_object()) {
            return R::err({K::Transient, "TCGdex EN card response is not a JSON object."});
        }
        if (!j.contains("image") || j.at("image").is_null()) {
            return R::err({K::NotFound, "TCGdex EN card has no image."});
        }
        if (!j.at("image").is_string()) {
            return R::err({K::Transient, "TCGdex EN card image field is not a string."});
        }
        const std::string base = j.at("image").get<std::string>();
        if (base.empty()) {
            return R::err({K::NotFound, "TCGdex EN card has no image."});
        }
        return R::ok(imageUrlFromBase(base));
    } catch (const std::exception& e) {
        return R::err({K::Transient,
                       std::string("TCGdex EN card JSON parse error: ") + e.what()});
    }
}

Result<std::string, PreviewLookupError>
PokemonCardPreviewSource::parseSearchResponse(const std::string& body) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.is_array()) {
            return R::err({K::Transient, "TCGdex EN cards search response is not an array."});
        }
        if (j.empty()) {
            return R::err({K::NotFound, "TCGdex EN returned no matching cards."});
        }
        for (const auto& card : j) {
            if (!card.contains("image") || !card.at("image").is_string()) continue;
            const std::string base = card.at("image").get<std::string>();
            if (base.empty()) continue;
            return R::ok(imageUrlFromBase(base));
        }
        return R::err({K::NotFound, "TCGdex EN matching cards have no image."});
    } catch (const std::exception& e) {
        return R::err({K::Transient,
                       std::string("TCGdex EN cards search JSON parse error: ") + e.what()});
    }
}

Result<std::string, PreviewLookupError>
PokemonCardPreviewSource::fetchImageUrl(std::string_view name,
                                        std::string_view setId,
                                        std::string_view setNo) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;

    const std::string idCanon = canonicalizeWestSetId(setId);
    const std::string num = normalizeCollectorNumber(setNo);
    if (!idCanon.empty() && !num.empty()) {
        auto byId = http_.get(buildCardByIdUrl(idCanon, num));
        if (byId) {
            auto img = parseCardByIdResponse(byId.value());
            if (img) return img;
            // NotFound / Transient schema: fall through to search.
        }
    }

    const std::string url = buildSearchUrl(name, idCanon, num);
    auto resp = http_.get(url);
    if (!resp) return R::err({K::Transient, resp.error()});
    return parseSearchResponse(resp.value());
}

Result<std::vector<AutoDetectedPrint>> PokemonCardPreviewSource::parsePrintVariants(
    const std::string& body,
    std::string_view /*setId*/,
    std::string_view wantedCardName) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    auto rows = parseSetCards(body);
    if (!rows) {
        return R::err(rows.error().message);
    }

    const std::string wantedLower = toLower(trim(std::string(wantedCardName)));
    std::vector<AutoDetectedPrint> out;
    std::unordered_set<std::string> seen;

    for (const auto& row : rows.value()) {
        if (!wantedLower.empty()) {
            if (toLower(trim(row.name)) != wantedLower) continue;
        }
        const std::string localId = normalizeCollectorNumber(row.localId);
        if (localId.empty() || !seen.insert(localId + '\0' + row.rarity).second) continue;
        AutoDetectedPrint print;
        print.setNo = localId;
        print.rarity = row.rarity;
        out.push_back(std::move(print));
    }

    if (out.empty()) {
        return R::err("Could not auto-detect set print metadata.");
    }
    return R::ok(std::move(out));
}

Result<AutoDetectedPrint> PokemonCardPreviewSource::detectFirstPrint(std::string_view name,
                                                                    std::string_view setId) {
    auto list = detectPrintVariants(name, setId);
    if (!list || list.value().empty()) {
        if (!list) return Result<AutoDetectedPrint>::err(list.error());
        return Result<AutoDetectedPrint>::err("Could not auto-detect set print metadata.");
    }
    return Result<AutoDetectedPrint>::ok(list.value().front());
}

Result<std::vector<AutoDetectedPrint>> PokemonCardPreviewSource::detectPrintVariants(
    std::string_view name,
    std::string_view setId) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    const std::string idCanon = canonicalizeWestSetId(setId);
    if (!idCanon.empty()) {
        auto detail = http_.get(buildSetDetailUrl(idCanon));
        if (detail) {
            auto parsed = parsePrintVariants(detail.value(), idCanon, name);
            if (parsed) return parsed;
        }
    }

    // Fallback: filtered cards search by name (+ optional set).
    const std::string url = buildSearchUrl(name, idCanon, "");
    auto resp = http_.get(url);
    if (!resp) return R::err(resp.error());

    try {
        const auto j = nlohmann::json::parse(resp.value());
        if (!j.is_array() || j.empty()) {
            return R::err("TCGdex EN returned no matching cards.");
        }
        const std::string wantedLower = toLower(trim(std::string(name)));
        std::vector<AutoDetectedPrint> collected;
        std::unordered_set<std::string> seen;
        for (const auto& card : j) {
            if (!wantedLower.empty()) {
                const std::string cardName = trim(card.value("name", ""));
                if (toLower(cardName) != wantedLower) continue;
            }
            if (!idCanon.empty()) {
                std::string cardSetId;
                if (card.contains("set") && card.at("set").is_object()) {
                    cardSetId = trim(card.at("set").value("id", ""));
                } else if (card.contains("id") && card.at("id").is_string()) {
                    // Slim search hits are "setId-localId".
                    const std::string id = card.at("id").get<std::string>();
                    const auto dash = id.rfind('-');
                    if (dash != std::string::npos) cardSetId = id.substr(0, dash);
                }
                if (cardSetId != idCanon) continue;
            }
            AutoDetectedPrint print;
            print.setNo = normalizeCollectorNumber(card.value("localId", ""));
            print.rarity = trim(card.value("rarity", ""));
            if (print.setNo.empty() && print.rarity.empty()) continue;
            const std::string key = print.setNo + '\0' + print.rarity;
            if (!seen.insert(key).second) continue;
            collected.push_back(std::move(print));
        }
        if (collected.empty()) {
            return R::err("Could not auto-detect set print metadata.");
        }
        return R::ok(std::move(collected));
    } catch (const std::exception& e) {
        return R::err(std::string("TCGdex EN cards search JSON parse error: ") + e.what());
    }
}

}  // namespace ccm
