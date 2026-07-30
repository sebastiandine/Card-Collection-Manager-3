#include "ccm/services/YuGiOhBandaiSetCompletion.hpp"

#include "ccm/games/yugiohbandai/YuGiOhBandaiSetSource.hpp"

#include <algorithm>
#include <array>
#include <unordered_map>
#include <unordered_set>

namespace ccm {

namespace {

using OwnedBySet = std::unordered_map<std::string, std::unordered_set<std::string>>;

bool passesLanguageFilter(const YuGiOhBandaiCard& card,
                          std::optional<Language> languageFilter) {
    return !languageFilter.has_value() || card.language == *languageFilter;
}

OwnedBySet ownedSetNosBySetId(const std::vector<YuGiOhBandaiCard>& collection,
                              std::optional<Language>              languageFilter) {
    OwnedBySet out;
    for (const auto& card : collection) {
        if (!passesLanguageFilter(card, languageFilter)) continue;
        if (card.set.id.empty()) continue;
        const std::string setNo = YuGiOhBandaiSetSource::normalizeCardNumber(card.setNo);
        if (setNo.empty()) continue;
        out[card.set.id].insert(setNo);
    }
    return out;
}

}  // namespace

std::vector<Language>
yuGiOhBandaiLanguagesInCollection(const std::vector<YuGiOhBandaiCard>& collection) {
    const auto& langs = allLanguages();
    std::array<bool, 10> present{};
    for (const auto& card : collection) {
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

std::vector<YuGiOhBandaiSetCompletionProgress>
computeYuGiOhBandaiSetCompletion(const std::vector<YuGiOhBandaiCard>& collection,
                                 const YuGiOhBandaiSetCatalog&        catalog,
                                 std::optional<Language>              languageFilter) {
    const OwnedBySet owned = ownedSetNosBySetId(collection, languageFilter);

    std::vector<YuGiOhBandaiSetCompletionProgress> out;
    out.reserve(owned.size());

    for (const auto& [setId, ownedNos] : owned) {
        const auto* pack = catalog.findPack(setId);
        if (pack == nullptr || pack->cards.empty()) continue;

        std::size_t matched = 0;
        for (const auto& card : pack->cards) {
            const std::string catalogNo =
                YuGiOhBandaiSetSource::normalizeCardNumber(card.setNo);
            if (!catalogNo.empty() && ownedNos.count(catalogNo) != 0) ++matched;
        }

        YuGiOhBandaiSetCompletionProgress row;
        row.setId = pack->setId;
        row.setName = pack->setName;
        row.ownedUnique = matched;
        row.total = pack->cards.size();
        out.push_back(std::move(row));
    }

    std::sort(out.begin(), out.end(),
              [](const YuGiOhBandaiSetCompletionProgress& a,
                 const YuGiOhBandaiSetCompletionProgress& b) {
                  return a.setName < b.setName;
              });
    return out;
}

std::vector<YuGiOhBandaiChecklistEntry>
yuGiOhBandaiChecklistForSet(const std::vector<YuGiOhBandaiCard>& collection,
                            const YuGiOhBandaiSetCatalog&        catalog,
                            std::string_view                     setId,
                            std::optional<Language>              languageFilter) {
    const auto* pack = catalog.findPack(setId);
    if (pack == nullptr) return {};

    std::unordered_set<std::string> ownedNos;
    for (const auto& card : collection) {
        if (!passesLanguageFilter(card, languageFilter)) continue;
        if (card.set.id != setId) continue;
        const std::string setNo = YuGiOhBandaiSetSource::normalizeCardNumber(card.setNo);
        if (!setNo.empty()) ownedNos.insert(setNo);
    }

    std::vector<YuGiOhBandaiChecklistEntry> out;
    out.reserve(pack->cards.size());
    for (const auto& card : pack->cards) {
        YuGiOhBandaiChecklistEntry entry;
        entry.setNo = YuGiOhBandaiSetSource::normalizeCardNumber(card.setNo);
        entry.name = card.name;
        entry.rarity = card.rarity;
        entry.owned = !entry.setNo.empty() && ownedNos.count(entry.setNo) != 0;
        out.push_back(std::move(entry));
    }

    std::sort(out.begin(), out.end(),
              [](const YuGiOhBandaiChecklistEntry& a,
                 const YuGiOhBandaiChecklistEntry& b) {
                  if (a.setNo != b.setNo) return a.setNo < b.setNo;
                  return a.name < b.name;
              });
    return out;
}

}  // namespace ccm
