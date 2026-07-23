#include "ccm/games/yugioh/YuGiOhSetSource.hpp"

#include "ccm/util/YuGiOhPrintingSlot.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <unordered_map>
#include <utility>

namespace ccm {
namespace {

struct YuGiOhSetAlias {
    const char* code;
    const char* name;
    const char* releaseDate;
};

constexpr std::array<YuGiOhSetAlias, 6> kMissing25thAnniversaryReprints{{
    // Keep this list in sync with docs/assets-and-info-apis.md (Info API section).
    {"LOB-25TH", "Legend of Blue Eyes White Dragon (25th Anniversary Edition)", "2023/04/20"},
    {"MRD-25TH", "Metal Raiders (25th Anniversary Edition)", "2023/04/20"},
    {"SRL-25TH", "Spell Ruler (25th Anniversary Edition)", "2023/04/20"},
    {"PSV-25TH", "Pharaoh's Servant (25th Anniversary Edition)", "2023/04/20"},
    {"DCR-25TH", "Dark Crisis (25th Anniversary Edition)", "2023/04/20"},
    {"IOC-25TH", "Invasion of Chaos (25th Anniversary Edition)", "2023/06/08"},
}};

void appendMissingSetAliases(std::vector<Set>& sets) {
    for (const auto& alias : kMissing25thAnniversaryReprints) {
        const bool exists = std::any_of(
            sets.begin(), sets.end(), [&](const Set& s) { return s.name == alias.name; });
        if (exists) continue;

        Set s;
        s.id = alias.code;
        s.name = alias.name;
        s.releaseDate = alias.releaseDate;
        sets.push_back(std::move(s));
    }
}

[[nodiscard]] std::string ygoSlotKey(std::string_view setNo) {
    const std::string abbrev = ygoAbbrevBeforeDash(setNo);
    const std::string digits = ygoCollectorDigitsOnly(setNo);
    if (abbrev.empty() || digits.empty()) return {};
    return abbrev + "|" + digits;
}

[[nodiscard]] bool ygoHasEnRegionInfix(std::string_view setCode) {
    const std::string_view s = trimAsciiSpaces(setCode);
    const auto dash = s.find('-');
    if (dash == std::string_view::npos || dash + 3 > s.size()) return false;
    const std::string_view tail = s.substr(dash + 1);
    if (tail.size() < 3) return false;
    return (tail[0] == 'E' || tail[0] == 'e') && (tail[1] == 'N' || tail[1] == 'n')
        && std::isdigit(static_cast<unsigned char>(tail[2])) != 0;
}

[[nodiscard]] std::string uppercaseAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}

[[nodiscard]] std::string resolvePackId(const std::unordered_map<std::string, std::string>& nameToId,
                                        const std::string& setName,
                                        const std::string& setCode) {
    const auto it = nameToId.find(setName);
    if (it != nameToId.end() && !it->second.empty()) return it->second;
    const std::string abbrev = uppercaseAscii(ygoAbbrevBeforeDash(setCode));
    return abbrev;
}

struct PackBuild {
    std::string setId;
    std::string setName;
    // slotKey → index into cards (for EN preference upgrades).
    std::unordered_map<std::string, std::size_t> slotIndex;
    std::vector<YuGiOhCatalogCard>               cards;
};

}  // namespace

YuGiOhSetSource::YuGiOhSetSource(IHttpClient& http) : http_(http) {}

Result<std::vector<Set>> YuGiOhSetSource::parseResponse(const std::string& body) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.is_array()) {
            return Result<std::vector<Set>>::err(
                "YGOPRODeck response is not an array.");
        }
        std::vector<Set> out;
        out.reserve(j.size());
        for (const auto& entry : j) {
            Set s;
            s.id          = entry.value("set_code", "");
            s.name        = entry.value("set_name", "");
            std::string release = entry.value("tcg_date", "");
            for (char& ch : release) {
                if (ch == '-') ch = '/';
            }
            s.releaseDate = std::move(release);
            out.push_back(std::move(s));
        }
        appendMissingSetAliases(out);
        std::sort(out.begin(), out.end(),
                  [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
        return Result<std::vector<Set>>::ok(std::move(out));
    } catch (const std::exception& e) {
        return Result<std::vector<Set>>::err(
            std::string("YGOPRODeck set parse error: ") + e.what());
    }
}

Result<YuGiOhSetCatalog> YuGiOhSetSource::parseCatalog(const std::string&      body,
                                                       const std::vector<Set>& sets) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.is_object() || !j.contains("data") || !j.at("data").is_array()) {
            return Result<YuGiOhSetCatalog>::err(
                "YGOPRODeck cardinfo response missing data array.");
        }

        std::unordered_map<std::string, std::string> nameToId;
        nameToId.reserve(sets.size());
        for (const auto& set : sets) {
            if (set.name.empty() || set.id.empty()) continue;
            // First wins — aliases and upstream rows rarely collide by name.
            nameToId.emplace(set.name, set.id);
        }

        // Keyed by pack setId.
        std::unordered_map<std::string, PackBuild> byId;

        for (const auto& cardJson : j.at("data")) {
            const std::string cardName = cardJson.value("name", "");
            if (cardName.empty()) continue;
            if (!cardJson.contains("card_sets") || !cardJson.at("card_sets").is_array()) {
                continue;
            }
            for (const auto& printing : cardJson.at("card_sets")) {
                const std::string setName = printing.value("set_name", "");
                const std::string setCode = printing.value("set_code", "");
                if (setName.empty() || setCode.empty()) continue;
                if (ygoLikelyEuropeanRegionalSetCode(setCode)) continue;

                const std::string slot = ygoSlotKey(setCode);
                if (slot.empty()) continue;

                const std::string packId = resolvePackId(nameToId, setName, setCode);
                if (packId.empty()) continue;

                auto& build = byId[packId];
                if (build.setId.empty()) {
                    build.setId = packId;
                    build.setName = setName;
                }

                const auto existing = build.slotIndex.find(slot);
                if (existing == build.slotIndex.end()) {
                    build.slotIndex.emplace(slot, build.cards.size());
                    build.cards.push_back(YuGiOhCatalogCard{setCode, cardName});
                    continue;
                }

                // Prefer an EN-embedded code over a bare / other-region equivalent.
                auto& prev = build.cards[existing->second];
                if (!ygoHasEnRegionInfix(prev.setNo) && ygoHasEnRegionInfix(setCode)) {
                    prev.setNo = setCode;
                    if (!cardName.empty()) prev.name = cardName;
                }
            }
        }

        YuGiOhSetCatalog catalog;
        catalog.packs.reserve(byId.size());
        for (auto& [_, build] : byId) {
            if (build.setId.empty() || build.cards.empty()) continue;
            std::sort(build.cards.begin(), build.cards.end(),
                      [](const YuGiOhCatalogCard& a, const YuGiOhCatalogCard& b) {
                          if (a.setNo != b.setNo) return a.setNo < b.setNo;
                          return a.name < b.name;
                      });
            YuGiOhSetCatalogPack pack;
            pack.setId = std::move(build.setId);
            pack.setName = std::move(build.setName);
            pack.cards = std::move(build.cards);
            catalog.packs.push_back(std::move(pack));
        }

        std::sort(catalog.packs.begin(), catalog.packs.end(),
                  [](const YuGiOhSetCatalogPack& a, const YuGiOhSetCatalogPack& b) {
                      return a.setName < b.setName;
                  });
        return Result<YuGiOhSetCatalog>::ok(std::move(catalog));
    } catch (const std::exception& e) {
        return Result<YuGiOhSetCatalog>::err(
            std::string("YGOPRODeck catalog parse error: ") + e.what());
    }
}

Result<std::vector<Set>> YuGiOhSetSource::fetchAll() {
    auto resp = http_.get(kEndpoint);
    if (!resp) return Result<std::vector<Set>>::err(resp.error());
    return parseResponse(resp.value());
}

Result<YuGiOhSetSource::FetchWithCatalog> YuGiOhSetSource::fetchAllWithCatalog() {
    auto setsResp = http_.get(kEndpoint);
    if (!setsResp) return Result<FetchWithCatalog>::err(setsResp.error());

    auto sets = parseResponse(setsResp.value());
    if (!sets) return Result<FetchWithCatalog>::err(sets.error());

    auto infoResp = http_.get(kCardInfoEndpoint);
    if (!infoResp) return Result<FetchWithCatalog>::err(infoResp.error());

    auto catalog = parseCatalog(infoResp.value(), sets.value());
    if (!catalog) return Result<FetchWithCatalog>::err(catalog.error());

    FetchWithCatalog out;
    out.sets = std::move(sets).value();
    out.catalog = std::move(catalog).value();
    return Result<FetchWithCatalog>::ok(std::move(out));
}

}  // namespace ccm
