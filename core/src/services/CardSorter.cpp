#include "ccm/services/CardSorter.hpp"

#include "ccm/domain/Enums.hpp"
#include "ccm/util/AsciiUtils.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace ccm {
namespace {

// Wrap a less-than predicate so that ascending=false flips its meaning,
// mirroring `byField(field, asc)` in TableTemplate.tsx.
template <typename Less>
auto directional(Less less, bool ascending) {
    return [less, ascending](const auto& a, const auto& b) {
        return ascending ? less(a, b) : less(b, a);
    };
}

}  // namespace

void sortMagicCards(std::vector<MagicCard>& cards, MagicSortColumn column,
                    bool ascending) {
    switch (column) {
    case MagicSortColumn::Name:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const MagicCard& a, const MagicCard& b) {
                return asciiLower(a.name) < asciiLower(b.name);
            }, ascending));
        break;
    case MagicSortColumn::SetReleaseDate:
        // Release dates are stored as "YYYY/MM/DD" so plain lexicographic
        // compare is chronological. The legacy JS path lowercased strings anyway; we
        // do the same for parity even though digits/'/' are unaffected.
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const MagicCard& a, const MagicCard& b) {
                return asciiLower(a.set.releaseDate) <
                       asciiLower(b.set.releaseDate);
            }, ascending));
        break;
    case MagicSortColumn::Language:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const MagicCard& a, const MagicCard& b) {
                return asciiLower(to_string(a.language)) <
                       asciiLower(to_string(b.language));
            }, ascending));
        break;
    case MagicSortColumn::Condition:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const MagicCard& a, const MagicCard& b) {
                return asciiLower(to_string(a.condition)) <
                       asciiLower(to_string(b.condition));
            }, ascending));
        break;
    case MagicSortColumn::Amount:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const MagicCard& a, const MagicCard& b) {
                return a.amount < b.amount;
            }, ascending));
        break;
    case MagicSortColumn::Foil:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const MagicCard& a, const MagicCard& b) {
                return a.foil < b.foil;  // false < true (asc puts unset first)
            }, ascending));
        break;
    case MagicSortColumn::Signed:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const MagicCard& a, const MagicCard& b) {
                return a.signed_ < b.signed_;
            }, ascending));
        break;
    case MagicSortColumn::Altered:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const MagicCard& a, const MagicCard& b) {
                return a.altered < b.altered;
            }, ascending));
        break;
    case MagicSortColumn::Note:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const MagicCard& a, const MagicCard& b) {
                return asciiLower(a.note) < asciiLower(b.note);
            }, ascending));
        break;
    }
}

void sortPokemonCards(std::vector<PokemonCard>& cards, PokemonSortColumn column,
                      bool ascending) {
    switch (column) {
    case PokemonSortColumn::Name:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const PokemonCard& a, const PokemonCard& b) {
                return asciiLower(a.name) < asciiLower(b.name);
            }, ascending));
        break;
    case PokemonSortColumn::SetReleaseDate:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const PokemonCard& a, const PokemonCard& b) {
                return asciiLower(a.set.releaseDate) <
                       asciiLower(b.set.releaseDate);
            }, ascending));
        break;
    case PokemonSortColumn::Language:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const PokemonCard& a, const PokemonCard& b) {
                return asciiLower(to_string(a.language)) <
                       asciiLower(to_string(b.language));
            }, ascending));
        break;
    case PokemonSortColumn::Condition:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const PokemonCard& a, const PokemonCard& b) {
                return asciiLower(to_string(a.condition)) <
                       asciiLower(to_string(b.condition));
            }, ascending));
        break;
    case PokemonSortColumn::Amount:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const PokemonCard& a, const PokemonCard& b) {
                return a.amount < b.amount;
            }, ascending));
        break;
    case PokemonSortColumn::Holo:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const PokemonCard& a, const PokemonCard& b) {
                return a.holo < b.holo;
            }, ascending));
        break;
    case PokemonSortColumn::FirstEdition:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const PokemonCard& a, const PokemonCard& b) {
                return a.firstEdition < b.firstEdition;
            }, ascending));
        break;
    case PokemonSortColumn::Signed:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const PokemonCard& a, const PokemonCard& b) {
                return a.signed_ < b.signed_;
            }, ascending));
        break;
    case PokemonSortColumn::Altered:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const PokemonCard& a, const PokemonCard& b) {
                return a.altered < b.altered;
            }, ascending));
        break;
    case PokemonSortColumn::Note:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const PokemonCard& a, const PokemonCard& b) {
                return asciiLower(a.note) < asciiLower(b.note);
            }, ascending));
        break;
    }
}

void sortYuGiOhCards(std::vector<YuGiOhCard>& cards, YuGiOhSortColumn column,
                     bool ascending) {
    switch (column) {
    case YuGiOhSortColumn::Name:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const YuGiOhCard& a, const YuGiOhCard& b) {
                return asciiLower(a.name) < asciiLower(b.name);
            }, ascending));
        break;
    case YuGiOhSortColumn::SetReleaseDate:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const YuGiOhCard& a, const YuGiOhCard& b) {
                return asciiLower(a.set.releaseDate) < asciiLower(b.set.releaseDate);
            }, ascending));
        break;
    case YuGiOhSortColumn::Language:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const YuGiOhCard& a, const YuGiOhCard& b) {
                return asciiLower(to_string(a.language)) < asciiLower(to_string(b.language));
            }, ascending));
        break;
    case YuGiOhSortColumn::Condition:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const YuGiOhCard& a, const YuGiOhCard& b) {
                return asciiLower(to_string(a.condition)) < asciiLower(to_string(b.condition));
            }, ascending));
        break;
    case YuGiOhSortColumn::Amount:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const YuGiOhCard& a, const YuGiOhCard& b) {
                return a.amount < b.amount;
            }, ascending));
        break;
    case YuGiOhSortColumn::FirstEdition:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const YuGiOhCard& a, const YuGiOhCard& b) {
                return a.firstEdition < b.firstEdition;
            }, ascending));
        break;
    case YuGiOhSortColumn::Signed:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const YuGiOhCard& a, const YuGiOhCard& b) {
                return a.signed_ < b.signed_;
            }, ascending));
        break;
    case YuGiOhSortColumn::Altered:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const YuGiOhCard& a, const YuGiOhCard& b) {
                return a.altered < b.altered;
            }, ascending));
        break;
    case YuGiOhSortColumn::Note:
        std::stable_sort(cards.begin(), cards.end(), directional(
            [](const YuGiOhCard& a, const YuGiOhCard& b) {
                return asciiLower(a.note) < asciiLower(b.note);
            }, ascending));
        break;
    }
}

}  // namespace ccm
