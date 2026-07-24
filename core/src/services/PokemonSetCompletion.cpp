#include "ccm/services/PokemonSetCompletion.hpp"

#include "ccm/games/pokemon/PokemonCardPreviewSource.hpp"
#include "ccm/games/pokemon/PokemonWestSetId.hpp"
#include "ccm/games/pokemonjp/JapanesePokemonCardPreviewSource.hpp"
#include "ccm/util/SetNoNatural.hpp"

#include <algorithm>
#include <array>
#include <unordered_map>
#include <unordered_set>

namespace ccm {

namespace {

struct OwnedSetInfo {
    std::unordered_set<std::string> nos;
    std::string                     releaseDate;
};

using OwnedBySet = std::unordered_map<std::string, OwnedSetInfo>;

bool passesLanguageFilter(const PokemonCard& card, std::optional<Language> languageFilter) {
    return !languageFilter.has_value() || card.language == *languageFilter;
}

bool passesRegionFilter(const PokemonCard& card, std::optional<PokemonRegion> regionFilter) {
    return !regionFilter.has_value() || card.region == *regionFilter;
}

std::string normalizeForRegion(PokemonRegion region, std::string_view setNo) {
    if (region == PokemonRegion::Asia) {
        return JapanesePokemonCardPreviewSource::normalizeLocalId(setNo);
    }
    return PokemonCardPreviewSource::normalizeCollectorNumber(setNo);
}

std::string westSetKey(std::string_view setId) {
    return canonicalizeWestSetId(setId);
}

OwnedBySet ownedSetNosBySetId(const std::vector<PokemonCard>& collection,
                              PokemonRegion                   region,
                              std::optional<Language>         languageFilter) {
    OwnedBySet out;
    for (const auto& card : collection) {
        if (card.region != region) continue;
        if (!passesLanguageFilter(card, languageFilter)) continue;
        if (card.set.id.empty()) continue;
        const std::string setNo = normalizeForRegion(region, card.setNo);
        if (setNo.empty()) continue;
        const std::string setKey =
            region == PokemonRegion::West ? westSetKey(card.set.id) : card.set.id;
        auto& info = out[setKey];
        info.nos.insert(setNo);
        if (info.releaseDate.empty() && !card.set.releaseDate.empty()) {
            info.releaseDate = card.set.releaseDate;
        }
    }
    return out;
}

std::vector<PokemonSetCompletionProgress>
computeForCatalog(const std::vector<PokemonCard>& collection,
                  const PokemonSetCatalog&        catalog,
                  PokemonRegion                   region,
                  std::optional<Language>         languageFilter) {
    const OwnedBySet owned = ownedSetNosBySetId(collection, region, languageFilter);

    std::vector<PokemonSetCompletionProgress> out;
    out.reserve(owned.size());

    for (const auto& [setId, info] : owned) {
        const auto* pack = catalog.findPack(setId);
        if (pack == nullptr || pack->cards.empty()) continue;

        std::size_t matched = 0;
        for (const auto& card : pack->cards) {
            const std::string catalogNo = normalizeForRegion(region, card.setNo);
            if (!catalogNo.empty() && info.nos.count(catalogNo) != 0) ++matched;
        }

        PokemonSetCompletionProgress row;
        row.region = region;
        row.setId = pack->setId;
        row.setName = pack->setName;
        row.releaseDate = info.releaseDate;
        row.ownedUnique = matched;
        row.total = pack->cards.size();
        out.push_back(std::move(row));
    }
    return out;
}

}  // namespace

std::vector<Language>
pokemonLanguagesInCollection(const std::vector<PokemonCard>& collection,
                             std::optional<PokemonRegion>    regionFilter) {
    const auto& langs = allLanguages();
    std::array<bool, 10> present{};
    for (const auto& card : collection) {
        if (!passesRegionFilter(card, regionFilter)) continue;
        for (std::size_t i = 0; i < langs.size(); ++i) {
            if (langs[i] == card.language) {
                present[i] = true;
                break;
            }
        }
    }

    std::vector<Language> out;
    for (std::size_t i = 0; i < langs.size(); ++i) {
        if (present[i]) out.push_back(langs[i]);
    }
    return out;
}

std::vector<PokemonRegion>
pokemonRegionsInCollection(const std::vector<PokemonCard>& collection,
                           const PokemonSetCatalog&        westCatalog,
                           const PokemonSetCatalog&        asiaCatalog) {
    std::vector<PokemonRegion> out;
    const auto westRows =
        computeForCatalog(collection, westCatalog, PokemonRegion::West, std::nullopt);
    if (!westRows.empty()) out.push_back(PokemonRegion::West);
    const auto asiaRows =
        computeForCatalog(collection, asiaCatalog, PokemonRegion::Asia, std::nullopt);
    if (!asiaRows.empty()) out.push_back(PokemonRegion::Asia);
    return out;
}

std::vector<PokemonSetCompletionProgress>
computePokemonSetCompletion(const std::vector<PokemonCard>& collection,
                            const PokemonSetCatalog&        westCatalog,
                            const PokemonSetCatalog&        asiaCatalog,
                            std::optional<PokemonRegion>    regionFilter,
                            std::optional<Language>         languageFilter) {
    std::vector<PokemonSetCompletionProgress> out;

    const bool includeWest =
        !regionFilter.has_value() || *regionFilter == PokemonRegion::West;
    const bool includeAsia =
        !regionFilter.has_value() || *regionFilter == PokemonRegion::Asia;

    if (includeWest) {
        auto west = computeForCatalog(collection, westCatalog, PokemonRegion::West,
                                      languageFilter);
        out.insert(out.end(), std::make_move_iterator(west.begin()),
                   std::make_move_iterator(west.end()));
    }
    if (includeAsia) {
        auto asia = computeForCatalog(collection, asiaCatalog, PokemonRegion::Asia,
                                      languageFilter);
        out.insert(out.end(), std::make_move_iterator(asia.begin()),
                   std::make_move_iterator(asia.end()));
    }

    std::sort(out.begin(), out.end(),
              [](const PokemonSetCompletionProgress& a,
                 const PokemonSetCompletionProgress& b) {
                  // YYYY/MM/DD lex order is chronological (CardSorter parity).
                  if (a.releaseDate != b.releaseDate) {
                      return a.releaseDate < b.releaseDate;
                  }
                  if (a.setName != b.setName) return a.setName < b.setName;
                  return static_cast<int>(a.region) < static_cast<int>(b.region);
              });
    return out;
}

std::vector<PokemonChecklistEntry>
pokemonChecklistForSet(const std::vector<PokemonCard>& collection,
                       const PokemonSetCatalog&        westCatalog,
                       const PokemonSetCatalog&        asiaCatalog,
                       PokemonRegion                   region,
                       std::string_view                setId,
                       std::optional<Language>         languageFilter) {
    const PokemonSetCatalog& catalog =
        region == PokemonRegion::Asia ? asiaCatalog : westCatalog;
    const std::string wantSetId =
        region == PokemonRegion::West ? westSetKey(setId) : std::string(setId);
    const auto* pack = catalog.findPack(wantSetId);
    if (pack == nullptr) return {};

    std::unordered_set<std::string> ownedNos;
    for (const auto& card : collection) {
        if (card.region != region) continue;
        if (!passesLanguageFilter(card, languageFilter)) continue;
        const std::string cardSetId =
            region == PokemonRegion::West ? westSetKey(card.set.id) : card.set.id;
        if (cardSetId != wantSetId) continue;
        const std::string setNo = normalizeForRegion(region, card.setNo);
        if (!setNo.empty()) ownedNos.insert(setNo);
    }

    std::vector<PokemonChecklistEntry> out;
    out.reserve(pack->cards.size());
    for (const auto& card : pack->cards) {
        PokemonChecklistEntry entry;
        entry.setNo = normalizeForRegion(region, card.setNo);
        entry.name = card.name;
        entry.owned = !entry.setNo.empty() && ownedNos.count(entry.setNo) != 0;
        out.push_back(std::move(entry));
    }

    std::sort(out.begin(), out.end(),
              [](const PokemonChecklistEntry& a, const PokemonChecklistEntry& b) {
                  const int cmp = compareSetNoNatural(a.setNo, b.setNo);
                  if (cmp != 0) return cmp < 0;
                  return a.name < b.name;
              });
    return out;
}

}  // namespace ccm
