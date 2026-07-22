#include "ccm/services/DigiBattle99SetCompletion.hpp"

#include "ccm/games/digibattle99/DigiBattle99CardPreviewSource.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace ccm {

namespace {

using OwnedBySet = std::unordered_map<std::string, std::unordered_set<std::string>>;

OwnedBySet ownedSetNosBySetId(const std::vector<DigiBattle99Card>& collection) {
    OwnedBySet out;
    for (const auto& card : collection) {
        if (card.set.id.empty()) continue;
        const std::string setNo =
            DigiBattle99CardPreviewSource::normalizeCardNumber(card.setNo);
        if (setNo.empty()) continue;
        out[card.set.id].insert(setNo);
    }
    return out;
}

}  // namespace

std::vector<DigiBattle99SetCompletionProgress>
computeDigiBattle99SetCompletion(const std::vector<DigiBattle99Card>& collection,
                                 const DigiBattle99SetCatalog&        catalog) {
    const OwnedBySet owned = ownedSetNosBySetId(collection);

    std::vector<DigiBattle99SetCompletionProgress> out;
    out.reserve(owned.size());

    for (const auto& [setId, ownedNos] : owned) {
        const auto* pack = catalog.findPack(setId);
        if (pack == nullptr || pack->cards.empty()) continue;

        std::size_t matched = 0;
        for (const auto& card : pack->cards) {
            const std::string catalogNo =
                DigiBattle99CardPreviewSource::normalizeCardNumber(card.setNo);
            if (!catalogNo.empty() && ownedNos.count(catalogNo) != 0) ++matched;
        }

        DigiBattle99SetCompletionProgress row;
        row.setId = pack->setId;
        row.setName = pack->setName;
        row.ownedUnique = matched;
        row.total = pack->cards.size();
        out.push_back(std::move(row));
    }

    std::sort(out.begin(), out.end(),
              [](const DigiBattle99SetCompletionProgress& a,
                 const DigiBattle99SetCompletionProgress& b) {
                  return a.setName < b.setName;
              });
    return out;
}

std::vector<DigiBattle99ChecklistEntry>
digiBattle99ChecklistForSet(const std::vector<DigiBattle99Card>& collection,
                            const DigiBattle99SetCatalog&        catalog,
                            std::string_view                     setId) {
    const auto* pack = catalog.findPack(setId);
    if (pack == nullptr) return {};

    std::unordered_set<std::string> ownedNos;
    for (const auto& card : collection) {
        if (card.set.id != setId) continue;
        const std::string setNo =
            DigiBattle99CardPreviewSource::normalizeCardNumber(card.setNo);
        if (!setNo.empty()) ownedNos.insert(setNo);
    }

    std::vector<DigiBattle99ChecklistEntry> out;
    out.reserve(pack->cards.size());
    for (const auto& card : pack->cards) {
        DigiBattle99ChecklistEntry entry;
        entry.setNo = DigiBattle99CardPreviewSource::normalizeCardNumber(card.setNo);
        entry.name = card.name;
        entry.owned = !entry.setNo.empty() && ownedNos.count(entry.setNo) != 0;
        out.push_back(std::move(entry));
    }

    std::sort(out.begin(), out.end(),
              [](const DigiBattle99ChecklistEntry& a,
                 const DigiBattle99ChecklistEntry& b) {
                  if (a.setNo != b.setNo) return a.setNo < b.setNo;
                  return a.name < b.name;
              });
    return out;
}

}  // namespace ccm
