#include "ccm/services/YuGiOhSetCompletion.hpp"

#include "ccm/util/YuGiOhPrintingSlot.hpp"

#include <algorithm>
#include <array>
#include <unordered_map>
#include <unordered_set>

namespace ccm {

namespace {

using OwnedBySet = std::unordered_map<std::string, std::unordered_set<std::string>>;

[[nodiscard]] std::string ygoSlotKey(std::string_view setNo) {
    const std::string abbrev = ygoAbbrevBeforeDash(setNo);
    const std::string digits = ygoCollectorDigitsOnly(setNo);
    if (abbrev.empty() || digits.empty()) return {};
    return abbrev + "|" + digits;
}

bool passesLanguageFilter(const YuGiOhCard& card, std::optional<Language> languageFilter) {
    return !languageFilter.has_value() || card.language == *languageFilter;
}

OwnedBySet ownedSlotsBySetId(const std::vector<YuGiOhCard>& collection,
                             std::optional<Language>        languageFilter) {
    OwnedBySet out;
    for (const auto& card : collection) {
        if (!passesLanguageFilter(card, languageFilter)) continue;
        if (card.set.id.empty()) continue;
        const std::string key = ygoSlotKey(card.setNo);
        if (key.empty()) continue;
        out[card.set.id].insert(key);
    }
    return out;
}

}  // namespace

std::vector<Language>
yuGiOhLanguagesInCollection(const std::vector<YuGiOhCard>& collection) {
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

std::vector<YuGiOhSetCompletionProgress>
computeYuGiOhSetCompletion(const std::vector<YuGiOhCard>& collection,
                           const YuGiOhSetCatalog&         catalog,
                           std::optional<Language>        languageFilter) {
    const OwnedBySet owned = ownedSlotsBySetId(collection, languageFilter);

    std::vector<YuGiOhSetCompletionProgress> out;
    out.reserve(owned.size());

    for (const auto& [setId, ownedSlots] : owned) {
        const auto* pack = catalog.findPack(setId);
        if (pack == nullptr || pack->cards.empty()) continue;

        std::size_t matched = 0;
        for (const auto& card : pack->cards) {
            const std::string key = ygoSlotKey(card.setNo);
            if (!key.empty() && ownedSlots.count(key) != 0) ++matched;
        }

        YuGiOhSetCompletionProgress row;
        row.setId = pack->setId;
        row.setName = pack->setName;
        row.ownedUnique = matched;
        row.total = pack->cards.size();
        out.push_back(std::move(row));
    }

    std::sort(out.begin(), out.end(),
              [](const YuGiOhSetCompletionProgress& a,
                 const YuGiOhSetCompletionProgress& b) {
                  return a.setName < b.setName;
              });
    return out;
}

std::vector<YuGiOhChecklistEntry>
yuGiOhChecklistForSet(const std::vector<YuGiOhCard>& collection,
                      const YuGiOhSetCatalog&         catalog,
                      std::string_view                setId,
                      std::optional<Language>         languageFilter) {
    const auto* pack = catalog.findPack(setId);
    if (pack == nullptr) return {};

    std::unordered_set<std::string> ownedSlots;
    for (const auto& card : collection) {
        if (!passesLanguageFilter(card, languageFilter)) continue;
        if (card.set.id != setId) continue;
        const std::string key = ygoSlotKey(card.setNo);
        if (!key.empty()) ownedSlots.insert(key);
    }

    std::vector<YuGiOhChecklistEntry> out;
    out.reserve(pack->cards.size());
    for (const auto& card : pack->cards) {
        YuGiOhChecklistEntry entry;
        entry.setNo = card.setNo;
        entry.name = card.name;
        const std::string key = ygoSlotKey(card.setNo);
        entry.owned = !key.empty() && ownedSlots.count(key) != 0;
        out.push_back(std::move(entry));
    }

    std::sort(out.begin(), out.end(),
              [](const YuGiOhChecklistEntry& a, const YuGiOhChecklistEntry& b) {
                  if (a.setNo != b.setNo) return a.setNo < b.setNo;
                  return a.name < b.name;
              });
    return out;
}

}  // namespace ccm
