#include "ccm/games/pokemon/PokemonSetSource.hpp"

#include "ccm/games/pokemon/PokemonCardPreviewSource.hpp"
#include "ccm/util/Rfc3986.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <unordered_set>
#include <utility>

namespace ccm {

PokemonSetSource::PokemonSetSource(IHttpClient& http) : http_(http) {}

std::string PokemonSetSource::rewriteReleaseDate(std::string_view isoDate) {
    std::string out(isoDate);
    for (char& ch : out) {
        if (ch == '-') ch = '/';
    }
    return out;
}

std::string PokemonSetSource::buildSetDetailUrl(std::string_view setId) {
    return std::string("https://api.tcgdex.net/v2/en/sets/") +
           rfc3986PercentEncode(setId);
}

Result<std::vector<Set>> PokemonSetSource::parseListResponse(const std::string& body) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (!j.is_array()) {
            return Result<std::vector<Set>>::err(
                "TCGdex EN sets response is not a JSON array.");
        }
        std::vector<Set> out;
        out.reserve(j.size());
        for (const auto& entry : j) {
            Set s;
            s.id = entry.value("id", "");
            if (s.id.empty()) continue;
            s.name = entry.value("name", "");
            s.releaseDate = {};  // filled from set detail
            out.push_back(std::move(s));
        }
        return Result<std::vector<Set>>::ok(std::move(out));
    } catch (const std::exception& e) {
        return Result<std::vector<Set>>::err(
            std::string("TCGdex EN sets JSON parse error: ") + e.what());
    }
}

Result<std::string> PokemonSetSource::parseReleaseDate(const std::string& detailBody) {
    try {
        const auto j = nlohmann::json::parse(detailBody);
        if (!j.is_object()) {
            return Result<std::string>::err(
                "TCGdex EN set detail response is not a JSON object.");
        }
        const std::string raw = j.value("releaseDate", "");
        if (raw.empty()) {
            return Result<std::string>::ok(std::string{});
        }
        return Result<std::string>::ok(rewriteReleaseDate(raw));
    } catch (const std::exception& e) {
        return Result<std::string>::err(
            std::string("TCGdex EN set detail JSON parse error: ") + e.what());
    }
}

Result<PokemonSetCatalogPack> PokemonSetSource::parseCatalogPackFromSetDetail(
    const std::string& detailBody,
    const Set&         set) {
    auto rows = PokemonCardPreviewSource::parseSetCards(detailBody);
    if (!rows) {
        return Result<PokemonSetCatalogPack>::err(rows.error().message);
    }

    PokemonSetCatalogPack pack;
    pack.setId = set.id;
    pack.setName = set.name.empty() ? set.id : set.name;

    std::unordered_set<std::string> seen;
    for (const auto& row : rows.value()) {
        const std::string localId =
            PokemonCardPreviewSource::normalizeCollectorNumber(row.localId);
        if (localId.empty() || !seen.insert(localId).second) continue;
        std::string name = row.name;
        if (name.empty()) name = localId;
        pack.cards.push_back(PokemonCatalogCard{localId, std::move(name)});
    }

    std::sort(pack.cards.begin(), pack.cards.end(),
              [](const PokemonCatalogCard& a, const PokemonCatalogCard& b) {
                  if (a.setNo != b.setNo) return a.setNo < b.setNo;
                  return a.name < b.name;
              });
    if (pack.cards.empty()) {
        return Result<PokemonSetCatalogPack>::err("No cards for set " + set.id);
    }
    return Result<PokemonSetCatalogPack>::ok(std::move(pack));
}

Result<std::vector<Set>> PokemonSetSource::fetchAll() {
    auto listResp = http_.get(kListEndpoint);
    if (!listResp) return Result<std::vector<Set>>::err(listResp.error());

    auto parsed = parseListResponse(listResp.value());
    if (!parsed) return parsed;

    std::vector<Set> out = std::move(parsed).value();
    for (auto& s : out) {
        auto detail = http_.get(buildSetDetailUrl(s.id));
        if (!detail) continue;  // keep set with empty date rather than fail all
        auto date = parseReleaseDate(detail.value());
        if (date && !date.value().empty()) {
            s.releaseDate = std::move(date).value();
        }
    }

    std::sort(out.begin(), out.end(),
              [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
    return Result<std::vector<Set>>::ok(std::move(out));
}

Result<PokemonSetSource::FetchWithCatalog> PokemonSetSource::fetchAllWithCatalog() {
    auto listResp = http_.get(kListEndpoint);
    if (!listResp) return Result<FetchWithCatalog>::err(listResp.error());

    auto parsed = parseListResponse(listResp.value());
    if (!parsed) return Result<FetchWithCatalog>::err(parsed.error());

    std::vector<Set> sets = std::move(parsed).value();
    PokemonSetCatalog catalog;
    catalog.packs.reserve(sets.size());

    for (auto& s : sets) {
        auto detail = http_.get(buildSetDetailUrl(s.id));
        if (!detail) continue;

        if (s.releaseDate.empty()) {
            auto date = parseReleaseDate(detail.value());
            if (date && !date.value().empty()) {
                s.releaseDate = std::move(date).value();
            }
        }

        auto pack = parseCatalogPackFromSetDetail(detail.value(), s);
        if (pack) {
            catalog.packs.push_back(std::move(pack).value());
        }
    }

    std::sort(sets.begin(), sets.end(),
              [](const Set& a, const Set& b) { return a.releaseDate < b.releaseDate; });
    std::sort(catalog.packs.begin(), catalog.packs.end(),
              [](const PokemonSetCatalogPack& a, const PokemonSetCatalogPack& b) {
                  return a.setName < b.setName;
              });

    FetchWithCatalog out;
    out.sets = std::move(sets);
    out.catalog = std::move(catalog);
    return Result<FetchWithCatalog>::ok(std::move(out));
}

}  // namespace ccm
