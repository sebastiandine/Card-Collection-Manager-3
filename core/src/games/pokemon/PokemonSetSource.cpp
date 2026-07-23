#include "ccm/games/pokemon/PokemonSetSource.hpp"

#include "ccm/games/pokemon/PokemonCardPreviewSource.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ccm {

namespace {

void finalizeCatalog(PokemonSetCatalog& catalog) {
    for (auto& pack : catalog.packs) {
        std::sort(pack.cards.begin(), pack.cards.end(),
                  [](const PokemonCatalogCard& a, const PokemonCatalogCard& b) {
                      if (a.setNo != b.setNo) return a.setNo < b.setNo;
                      return a.name < b.name;
                  });
    }
    std::sort(catalog.packs.begin(), catalog.packs.end(),
              [](const PokemonSetCatalogPack& a, const PokemonSetCatalogPack& b) {
                  return a.setName < b.setName;
              });
}

}  // namespace

PokemonSetSource::PokemonSetSource(IHttpClient& http) : http_(http) {}

Result<std::vector<Set>> PokemonSetSource::parseResponse(const std::string& body) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array()) {
            return Result<std::vector<Set>>::err(
                "Pokemon TCG API response missing 'data' array.");
        }
        std::vector<Set> out;
        out.reserve(j.at("data").size());
        for (const auto& entry : j.at("data")) {
            Set s;
            s.id          = entry.value("id", "");
            s.name        = entry.value("name", "");
            // Pokemon TCG API already returns "releaseDate" in YYYY/MM/DD;
            // no separator rewrite needed (cf. Scryfall's "released_at").
            s.releaseDate = entry.value("releaseDate", "");
            out.push_back(std::move(s));
        }
        std::sort(out.begin(), out.end(),
                  [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
        return Result<std::vector<Set>>::ok(std::move(out));
    } catch (const std::exception& e) {
        return Result<std::vector<Set>>::err(
            std::string("Pokemon TCG JSON parse error: ") + e.what());
    }
}

std::string PokemonSetSource::buildCardsPageUrl(int page, int pageSize) {
    return std::string(kCardsEndpoint) + "?select=name,number,set&pageSize=" +
           std::to_string(pageSize) + "&page=" + std::to_string(page);
}

Result<PokemonSetSource::CardsPageMeta>
PokemonSetSource::mergeCardsPage(const std::string&    body,
                                 PokemonSetCatalog&    catalog,
                                 const std::vector<Set>& sets) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.contains("data") || !j.at("data").is_array()) {
            return Result<CardsPageMeta>::err(
                "Pokemon TCG cards response missing 'data' array.");
        }

        std::unordered_map<std::string, std::string> idToName;
        idToName.reserve(sets.size());
        for (const auto& set : sets) {
            if (!set.id.empty()) idToName.emplace(set.id, set.name);
        }

        // Index existing packs for multi-page merges.
        std::unordered_map<std::string, std::size_t> packIndex;
        for (std::size_t i = 0; i < catalog.packs.size(); ++i) {
            packIndex.emplace(catalog.packs[i].setId, i);
        }
        std::vector<std::unordered_set<std::string>> seenByPack(catalog.packs.size());
        for (std::size_t i = 0; i < catalog.packs.size(); ++i) {
            for (const auto& card : catalog.packs[i].cards) {
                seenByPack[i].insert(card.setNo);
            }
        }

        for (const auto& entry : j.at("data")) {
            const std::string name = entry.value("name", "");
            const std::string number =
                PokemonCardPreviewSource::normalizeCollectorNumber(entry.value("number", ""));
            if (name.empty() || number.empty()) continue;

            std::string setId;
            std::string setName;
            if (entry.contains("set") && entry.at("set").is_object()) {
                setId = entry.at("set").value("id", "");
                setName = entry.at("set").value("name", "");
            }
            if (setId.empty()) continue;
            if (const auto it = idToName.find(setId); it != idToName.end() && !it->second.empty()) {
                setName = it->second;
            }
            if (setName.empty()) setName = setId;

            auto pit = packIndex.find(setId);
            if (pit == packIndex.end()) {
                PokemonSetCatalogPack pack;
                pack.setId = setId;
                pack.setName = setName;
                pack.cards.push_back(PokemonCatalogCard{number, name});
                packIndex.emplace(setId, catalog.packs.size());
                seenByPack.emplace_back(std::unordered_set<std::string>{number});
                catalog.packs.push_back(std::move(pack));
                continue;
            }

            const std::size_t idx = pit->second;
            if (!seenByPack[idx].insert(number).second) continue;
            if (catalog.packs[idx].setName.empty() && !setName.empty()) {
                catalog.packs[idx].setName = setName;
            }
            catalog.packs[idx].cards.push_back(PokemonCatalogCard{number, name});
        }

        CardsPageMeta meta;
        meta.page = j.value("page", 1);
        meta.pageSize = j.value("pageSize", kCardsPageSize);
        meta.count = j.value("count", static_cast<int>(j.at("data").size()));
        meta.totalCount = j.value("totalCount", meta.count);
        return Result<CardsPageMeta>::ok(meta);
    } catch (const std::exception& e) {
        return Result<CardsPageMeta>::err(
            std::string("Pokemon TCG cards JSON parse error: ") + e.what());
    }
}

Result<PokemonSetCatalog> PokemonSetSource::parseCatalog(const std::string&    body,
                                                         const std::vector<Set>& sets) {
    PokemonSetCatalog catalog;
    auto meta = mergeCardsPage(body, catalog, sets);
    if (!meta) return Result<PokemonSetCatalog>::err(meta.error());
    finalizeCatalog(catalog);
    return Result<PokemonSetCatalog>::ok(std::move(catalog));
}

Result<std::vector<Set>> PokemonSetSource::fetchAll() {
    auto resp = http_.get(kEndpoint);
    if (!resp) return Result<std::vector<Set>>::err(resp.error());
    return parseResponse(resp.value());
}

Result<PokemonSetSource::FetchWithCatalog> PokemonSetSource::fetchAllWithCatalog() {
    auto setsResp = http_.get(kEndpoint);
    if (!setsResp) return Result<FetchWithCatalog>::err(setsResp.error());
    auto sets = parseResponse(setsResp.value());
    if (!sets) return Result<FetchWithCatalog>::err(sets.error());

    PokemonSetCatalog catalog;
    int page = 1;
    int totalCount = 0;
    int fetched = 0;
    for (;;) {
        auto cardsResp = http_.get(buildCardsPageUrl(page));
        if (!cardsResp) return Result<FetchWithCatalog>::err(cardsResp.error());
        auto meta = mergeCardsPage(cardsResp.value(), catalog, sets.value());
        if (!meta) return Result<FetchWithCatalog>::err(meta.error());

        fetched += meta.value().count;
        totalCount = meta.value().totalCount;
        if (meta.value().count <= 0 || fetched >= totalCount) break;
        ++page;
        // Safety: avoid unbounded loops if the API lies about totals.
        if (page > 10000) {
            return Result<FetchWithCatalog>::err(
                "Pokemon TCG cards pagination exceeded safety limit.");
        }
    }

    finalizeCatalog(catalog);
    FetchWithCatalog out;
    out.sets = std::move(sets).value();
    out.catalog = std::move(catalog);
    return Result<FetchWithCatalog>::ok(std::move(out));
}

}  // namespace ccm
