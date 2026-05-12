#include "ccm/games/pokemon/PokemonCardPreviewSource.hpp"

#include "ccm/util/Rfc3986.hpp"

#include <nlohmann/json.hpp>

#include <cctype>
#include <string>
#include <unordered_set>
#include <vector>

namespace ccm {

namespace {

// Strip everything after the first '/' in a Pokemon collector number.
// The Pokemon TCG API expects `number:"4"`, but cards are commonly stored as
// `4/102`. Without this, no API match is found.
std::string normalizeNumber(std::string_view setNo) {
    std::string s(setNo);
    const auto slash = s.find('/');
    if (slash != std::string::npos) {
        s = s.substr(0, slash);
    }
    return s;
}

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

std::string PokemonCardPreviewSource::buildSearchUrl(std::string_view name,
                                                     std::string_view setId,
                                                     std::string_view setNo) {
    // Build the unencoded query first so the output matches what the Pokemon
    // TCG search syntax expects: name:"<name>" set.id:<setId> number:<num>.
    std::string query = "name:\"";
    query += std::string(name);
    query += "\"";
    if (!setId.empty()) {
        query += " set.id:";
        query += std::string(setId);
    }
    const std::string num = normalizeNumber(setNo);
    if (!num.empty()) {
        query += " number:";
        query += num;
    }
    return std::string("https://api.pokemontcg.io/v2/cards?q=") +
           rfc3986PercentEncode(query);
}

std::string PokemonCardPreviewSource::buildDetectSearchUrl(std::string_view name,
                                                           std::string_view setId) {
    std::string url = buildSearchUrl(name, setId, "");
    url += "&select=name,number,rarity,set";
    url += "&pageSize=50";
    return url;
}

Result<std::string, PreviewLookupError>
PokemonCardPreviewSource::parseResponse(const std::string& body) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array()) {
            return R::err({K::Transient, "Pokemon TCG response missing 'data' array."});
        }
        const auto& data = j.at("data");
        if (data.empty()) {
            return R::err({K::NotFound, "Pokemon TCG returned no matching cards."});
        }
        const auto& first = data.at(0);
        if (!first.contains("images") || !first.at("images").is_object()) {
            return R::err({K::NotFound, "Card has no 'images' object."});
        }
        const auto& images = first.at("images");
        if (images.contains("large") && images.at("large").is_string()) {
            return R::ok(images.at("large").get<std::string>());
        }
        if (images.contains("small") && images.at("small").is_string()) {
            return R::ok(images.at("small").get<std::string>());
        }
        return R::err({K::NotFound, "Card has no 'large' or 'small' image variant."});
    } catch (const std::exception& e) {
        return R::err({K::Transient,
            std::string("Pokemon TCG JSON parse error: ") + e.what()});
    }
}

Result<std::string, PreviewLookupError>
PokemonCardPreviewSource::fetchImageUrl(std::string_view name,
                                        std::string_view setId,
                                        std::string_view setNo) {
    using R = Result<std::string, PreviewLookupError>;
    using K = PreviewLookupError::Kind;
    const std::string url = buildSearchUrl(name, setId, setNo);
    auto resp = http_.get(url);
    if (!resp) return R::err({K::Transient, resp.error()});
    return parseResponse(resp.value());
}

Result<std::vector<AutoDetectedPrint>> PokemonCardPreviewSource::parsePrintVariants(
    const std::string& body,
    std::string_view setId,
    std::string_view wantedCardName) {
    using R = Result<std::vector<AutoDetectedPrint>>;
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array() || j.at("data").empty()) {
            return R::err("Pokemon TCG returned no matching cards.");
        }
        const std::string wantedSetId = trim(std::string(setId));
        const std::string wantedNameLower = toLower(trim(std::string(wantedCardName)));

        std::vector<AutoDetectedPrint> collected;
        auto pushCard = [&collected](const nlohmann::json& card) {
            AutoDetectedPrint out;
            out.setNo = trim(card.value("number", ""));
            out.rarity = trim(card.value("rarity", ""));
            if (out.setNo.empty() && out.rarity.empty()) return;
            collected.push_back(std::move(out));
        };

        for (const auto& card : j.at("data")) {
            if (!wantedNameLower.empty()) {
                const std::string cardName = trim(card.value("name", ""));
                if (toLower(cardName) != wantedNameLower) continue;
            }
            if (!wantedSetId.empty()) {
                std::string cardSetId;
                if (card.contains("set") && card.at("set").is_object()) {
                    cardSetId = trim(card.at("set").value("id", ""));
                }
                if (cardSetId != wantedSetId) continue;
            }
            pushCard(card);
        }

        if (collected.empty()) {
            if (!wantedNameLower.empty() && !wantedSetId.empty()) {
                return R::err("Could not auto-detect set print metadata.");
            }
            return R::err("Pokemon TCG returned no matching cards.");
        }

        std::vector<AutoDetectedPrint> deduped;
        deduped.reserve(collected.size());
        std::unordered_set<std::string> seen;
        seen.reserve(collected.size() * 2);
        for (auto& p : collected) {
            const std::string key = p.setNo + '\0' + p.rarity;
            if (seen.insert(key).second) deduped.push_back(std::move(p));
        }
        return R::ok(std::move(deduped));
    } catch (const std::exception& e) {
        return R::err(std::string("Pokemon TCG JSON parse error: ") + e.what());
    }
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
    const std::string url = buildDetectSearchUrl(name, setId);
    auto resp = http_.get(url);
    if (resp) {
        return parsePrintVariants(resp.value(), setId, name);
    }
    const std::string fallbackUrl = buildDetectSearchUrl(name, "");
    auto fallback = http_.get(fallbackUrl);
    if (!fallback) return R::err(fallback.error());
    return parsePrintVariants(fallback.value(), setId, name);
}

}  // namespace ccm
