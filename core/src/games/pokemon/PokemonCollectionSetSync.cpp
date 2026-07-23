#include "ccm/games/pokemon/PokemonCollectionSetSync.hpp"

#include "ccm/games/pokemon/PokemonWestSetId.hpp"

#include <unordered_map>

namespace ccm {

namespace {

std::unordered_map<std::string, const Set*> indexById(const std::vector<Set>& sets) {
    std::unordered_map<std::string, const Set*> out;
    out.reserve(sets.size());
    for (const auto& s : sets) {
        if (s.id.empty()) continue;
        out.emplace(s.id, &s);
    }
    return out;
}

bool applySetMetadata(PokemonCard& card, const Set& upstream) {
    bool changed = false;
    if (card.set.name != upstream.name) {
        card.set.name = upstream.name;
        changed = true;
    }
    if (card.set.releaseDate != upstream.releaseDate) {
        card.set.releaseDate = upstream.releaseDate;
        changed = true;
    }
    return changed;
}

}  // namespace

std::size_t syncPokemonCollectionSets(std::vector<PokemonCard>& cards,
                                      const std::vector<Set>&   westSets,
                                      const std::vector<Set>&   asiaSets) {
    const auto westById = indexById(westSets);
    const auto asiaById = indexById(asiaSets);

    std::size_t touched = 0;
    for (auto& card : cards) {
        bool changed = false;
        if (card.region == PokemonRegion::West) {
            if (!card.set.id.empty()) {
                const std::string canon = canonicalizeWestSetId(card.set.id);
                if (canon != card.set.id) {
                    card.set.id = canon;
                    changed = true;
                }
            }
            if (!card.set.id.empty()) {
                if (const auto it = westById.find(card.set.id); it != westById.end()) {
                    if (applySetMetadata(card, *it->second)) changed = true;
                }
            }
        } else if (card.region == PokemonRegion::Asia) {
            if (!card.set.id.empty()) {
                if (const auto it = asiaById.find(card.set.id); it != asiaById.end()) {
                    if (applySetMetadata(card, *it->second)) changed = true;
                }
            }
        }
        if (changed) ++touched;
    }
    return touched;
}

}  // namespace ccm
