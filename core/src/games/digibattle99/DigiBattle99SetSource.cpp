#include "ccm/games/digibattle99/DigiBattle99SetSource.hpp"

#include "ccm/games/digibattle99/DigiBattle99CardPreviewSource.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ccm {

namespace {

// Curated EN release dates for the vintage Digi-Battle product line.
// Series 1 Starter is verified 1999-06-01; other entries use digimoncard.io /
// checklist years (day unknown -> YYYY/01/01 or mid-year anchors for ordering).
const std::unordered_map<std::string, std::string>& curatedReleaseDates() {
    static const std::unordered_map<std::string, std::string> kDates{
        {"Series 1 Starter Set", "1999/06/01"},
        {"Series 1 Booster Pack", "1999/06/01"},
        {"Series 2 Booster Pack", "1999/09/01"},
        {"Series 3 Booster Pack", "2000/01/01"},
        {"Series 4 Booster Pack", "2000/06/01"},
        {"Series 5 Booster Pack", "2000/10/01"},
        {"Series 6 Booster Pack", "2001/01/01"},
        {"Street Starter Set 1", "2001/01/01"},
        {"Street Starter Set 2", "2001/02/01"},
        {"Street Starter Set 3", "2001/03/01"},
        {"Street Starter Set 4", "2001/04/01"},
        {"Digimon The Movie Promo Cards", "2000/10/01"},
    };
    return kDates;
}

std::string releaseDateForPack(const std::string& packName) {
    const auto& dates = curatedReleaseDates();
    const auto it = dates.find(packName);
    if (it != dates.end()) return it->second;
    return {};
}

Result<nlohmann::json> parseSearchArray(const std::string& body) {
    try {
        const auto j = nlohmann::json::parse(body);
        if (j.is_object() && j.contains("error")) {
            return Result<nlohmann::json>::err(
                j.value("error", std::string{"digimoncard.io set search error"}));
        }
        if (!j.is_array()) {
            return Result<nlohmann::json>::err(
                "digimoncard.io Digi-Battle response is not a JSON array.");
        }
        return Result<nlohmann::json>::ok(j);
    } catch (const std::exception& e) {
        return Result<nlohmann::json>::err(
            std::string("digimoncard.io Digi-Battle JSON parse error: ") + e.what());
    }
}

}  // namespace

DigiBattle99SetSource::DigiBattle99SetSource(IHttpClient& http) : http_(http) {}

std::string DigiBattle99SetSource::slugifyPackName(std::string_view packName) {
    std::string out;
    out.reserve(packName.size());
    bool pendingHyphen = false;
    for (unsigned char ch : packName) {
        if (std::isalnum(ch)) {
            if (pendingHyphen && !out.empty()) out.push_back('-');
            pendingHyphen = false;
            out.push_back(static_cast<char>(std::tolower(ch)));
        } else {
            pendingHyphen = !out.empty();
        }
    }
    return out;
}

Result<std::vector<Set>> DigiBattle99SetSource::parseResponse(const std::string& body) {
    auto arr = parseSearchArray(body);
    if (!arr) return Result<std::vector<Set>>::err(arr.error());

    // Preserve first-seen order of pack names, then sort by release date.
    std::unordered_set<std::string> seen;
    std::vector<std::string> packNames;
    packNames.reserve(16);
    for (const auto& entry : arr.value()) {
        if (!entry.contains("set_name") || !entry.at("set_name").is_array()) continue;
        for (const auto& pack : entry.at("set_name")) {
            if (!pack.is_string()) continue;
            const std::string name = pack.get<std::string>();
            if (name.empty()) continue;
            if (seen.insert(name).second) packNames.push_back(name);
        }
    }

    std::vector<Set> out;
    out.reserve(packNames.size());
    for (const auto& name : packNames) {
        Set s;
        s.id = slugifyPackName(name);
        s.name = name;
        s.releaseDate = releaseDateForPack(name);
        if (s.id.empty()) continue;
        out.push_back(std::move(s));
    }

    std::sort(out.begin(), out.end(), [](const Set& a, const Set& b) {
        if (a.releaseDate.empty() && !b.releaseDate.empty()) return false;
        if (!a.releaseDate.empty() && b.releaseDate.empty()) return true;
        if (a.releaseDate != b.releaseDate) return a.releaseDate < b.releaseDate;
        return a.name < b.name;
    });
    return Result<std::vector<Set>>::ok(std::move(out));
}

Result<DigiBattle99SetCatalog> DigiBattle99SetSource::parseCatalog(const std::string& body) {
    auto arr = parseSearchArray(body);
    if (!arr) return Result<DigiBattle99SetCatalog>::err(arr.error());

    // pack display name -> (setId, ordered unique cards by first-seen setNo)
    struct PackBuild {
        std::string setId;
        std::string setName;
        std::unordered_set<std::string> seenNos;
        std::vector<DigiBattle99CatalogCard> cards;
    };
    std::unordered_map<std::string, PackBuild> byName;

    for (const auto& entry : arr.value()) {
        if (!entry.contains("name") || !entry.at("name").is_string()) continue;
        if (!entry.contains("id") || !entry.at("id").is_string()) continue;
        if (!entry.contains("set_name") || !entry.at("set_name").is_array()) continue;

        DigiBattle99CatalogCard card;
        card.name = entry.at("name").get<std::string>();
        card.setNo = DigiBattle99CardPreviewSource::normalizeCardNumber(
            entry.at("id").get<std::string>());
        if (card.setNo.empty()) continue;

        for (const auto& pack : entry.at("set_name")) {
            if (!pack.is_string()) continue;
            const std::string packName = pack.get<std::string>();
            if (packName.empty()) continue;

            auto& build = byName[packName];
            if (build.setName.empty()) {
                build.setName = packName;
                build.setId = slugifyPackName(packName);
            }
            if (build.setId.empty()) continue;
            if (!build.seenNos.insert(card.setNo).second) continue;
            build.cards.push_back(card);
        }
    }

    DigiBattle99SetCatalog catalog;
    catalog.packs.reserve(byName.size());
    for (auto& [_, build] : byName) {
        if (build.setId.empty()) continue;
        std::sort(build.cards.begin(), build.cards.end(),
                  [](const DigiBattle99CatalogCard& a, const DigiBattle99CatalogCard& b) {
                      if (a.setNo != b.setNo) return a.setNo < b.setNo;
                      return a.name < b.name;
                  });
        DigiBattle99SetCatalogPack pack;
        pack.setId = std::move(build.setId);
        pack.setName = std::move(build.setName);
        pack.cards = std::move(build.cards);
        catalog.packs.push_back(std::move(pack));
    }

    std::sort(catalog.packs.begin(), catalog.packs.end(),
              [](const DigiBattle99SetCatalogPack& a, const DigiBattle99SetCatalogPack& b) {
                  return a.setName < b.setName;
              });
    return Result<DigiBattle99SetCatalog>::ok(std::move(catalog));
}

Result<DigiBattle99SetSource::FetchWithCatalog>
DigiBattle99SetSource::fetchAllWithCatalog() {
    auto resp = http_.get(kEndpoint);
    if (!resp) return Result<FetchWithCatalog>::err(resp.error());

    auto sets = parseResponse(resp.value());
    if (!sets) return Result<FetchWithCatalog>::err(sets.error());
    auto catalog = parseCatalog(resp.value());
    if (!catalog) return Result<FetchWithCatalog>::err(catalog.error());

    FetchWithCatalog out;
    out.sets = std::move(sets).value();
    out.catalog = std::move(catalog).value();
    return Result<FetchWithCatalog>::ok(std::move(out));
}

Result<std::vector<Set>> DigiBattle99SetSource::fetchAll() {
    auto both = fetchAllWithCatalog();
    if (!both) return Result<std::vector<Set>>::err(both.error());
    return Result<std::vector<Set>>::ok(std::move(both).value().sets);
}

}  // namespace ccm
