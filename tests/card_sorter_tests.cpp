// CardSorter tests - exercises the per-column comparators ported from the
// TableTemplate.tsx::byField. Each TEST_CASE pins down a specific behavior
// the original UI relied on, so a regression here implies the C++ table no
// longer behaves like the TypeScript reference.

#include <doctest/doctest.h>

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/MagicCard.hpp"
#include "ccm/domain/PokemonCard.hpp"
#include "ccm/domain/YuGiOhCard.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/services/CardSorter.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace ccm;

namespace {

MagicCard mc(std::uint32_t id, std::string name,
             std::string setName, std::string releaseDate,
             std::uint8_t amount = 1,
             bool foil = false, bool sgnd = false, bool altered = false,
             Language lang = Language::English,
             Condition cond = Condition::NearMint,
             std::string note = "") {
    MagicCard c;
    c.id = id;
    c.name = std::move(name);
    c.set.name = std::move(setName);
    c.set.releaseDate = std::move(releaseDate);
    c.amount = amount;
    c.foil = foil;
    c.signed_ = sgnd;
    c.altered = altered;
    c.language = lang;
    c.condition = cond;
    c.note = std::move(note);
    return c;
}

PokemonCard pc(std::uint32_t id, std::string name,
               std::string setName, std::string releaseDate,
               std::uint8_t amount = 1,
               bool holo = false, bool firstEdition = false,
               bool sgnd = false, bool altered = false) {
    PokemonCard c;
    c.id = id;
    c.name = std::move(name);
    c.set.name = std::move(setName);
    c.set.releaseDate = std::move(releaseDate);
    c.amount = amount;
    c.holo = holo;
    c.firstEdition = firstEdition;
    c.signed_ = sgnd;
    c.altered = altered;
    return c;
}

YuGiOhCard yc(std::uint32_t id, std::string name,
              std::string setName, std::string releaseDate,
              std::string setNo = "",
              std::string rarity = "",
              std::uint8_t amount = 1) {
    YuGiOhCard c;
    c.id = id;
    c.name = std::move(name);
    c.set.name = std::move(setName);
    c.set.releaseDate = std::move(releaseDate);
    c.setNo = std::move(setNo);
    c.rarity = std::move(rarity);
    c.amount = amount;
    return c;
}

std::vector<std::uint32_t> ids(const std::vector<MagicCard>& v) {
    std::vector<std::uint32_t> out;
    out.reserve(v.size());
    for (const auto& c : v) out.push_back(c.id);
    return out;
}

std::vector<std::uint32_t> ids(const std::vector<PokemonCard>& v) {
    std::vector<std::uint32_t> out;
    out.reserve(v.size());
    for (const auto& c : v) out.push_back(c.id);
    return out;
}

std::vector<std::uint32_t> ids(const std::vector<YuGiOhCard>& v) {
    std::vector<std::uint32_t> out;
    out.reserve(v.size());
    for (const auto& c : v) out.push_back(c.id);
    return out;
}

}  // namespace

TEST_SUITE("CardSorter - Magic columns") {
    TEST_CASE("Name sorts case-insensitively (matches String.toLowerCase())") {
        // Three cards whose names only differ in casing - if sort were a
        // plain `<` this would put "ABC" before "abc" and wedge "Brainstorm"
        // somewhere wrong. The JS path normalizes to lowercase first.
        std::vector<MagicCard> v = {
            mc(1, "brainstorm", "X", "2000/01/01"),
            mc(2, "ABC",        "X", "2000/01/01"),
            mc(3, "abc",        "X", "2000/01/01"),
        };
        sortMagicCards(v, MagicSortColumn::Name, /*ascending=*/true);
        // ABC and abc tie under case-insensitive compare; stable sort keeps
        // the input order (id 2 before id 3).
        CHECK(ids(v) == std::vector<std::uint32_t>{2, 3, 1});

        sortMagicCards(v, MagicSortColumn::Name, /*ascending=*/false);
        CHECK(ids(v) == std::vector<std::uint32_t>{1, 2, 3});
    }

    TEST_CASE("Set column sorts by release date (chronological), not by name") {
        // The whole point of having distinct valueKey/sortKey: the
        // table *displays* set.name, but ascending order is chronological.
        std::vector<MagicCard> v = {
            mc(1, "x", "Zendikar",       "2009/10/02"),
            mc(2, "y", "Alpha",          "1993/08/05"),
            mc(3, "z", "Modern Horizons","2019/06/14"),
        };
        sortMagicCards(v, MagicSortColumn::SetReleaseDate, /*ascending=*/true);
        CHECK(ids(v) == std::vector<std::uint32_t>{2, 1, 3});  // 1993 < 2009 < 2019

        sortMagicCards(v, MagicSortColumn::SetReleaseDate, /*ascending=*/false);
        CHECK(ids(v) == std::vector<std::uint32_t>{3, 1, 2});
    }

    TEST_CASE("Amount sorts numerically (no string-compare 10 < 2 trap)") {
        std::vector<MagicCard> v = {
            mc(1, "a", "X", "2000/01/01", /*amount=*/10),
            mc(2, "b", "X", "2000/01/01", /*amount=*/2),
            mc(3, "c", "X", "2000/01/01", /*amount=*/4),
        };
        sortMagicCards(v, MagicSortColumn::Amount, /*ascending=*/true);
        CHECK(ids(v) == std::vector<std::uint32_t>{2, 3, 1});  // 2 < 4 < 10
    }

    TEST_CASE("boolean flag column orders false < true (asc puts unset first)") {
        std::vector<MagicCard> v = {
            mc(1, "a", "X", "2000/01/01", 1, /*foil=*/true),
            mc(2, "b", "X", "2000/01/01", 1, /*foil=*/false),
            mc(3, "c", "X", "2000/01/01", 1, /*foil=*/true),
            mc(4, "d", "X", "2000/01/01", 1, /*foil=*/false),
        };
        sortMagicCards(v, MagicSortColumn::Foil, /*ascending=*/true);
        // Stable: relative order within each bucket preserved.
        CHECK(ids(v) == std::vector<std::uint32_t>{2, 4, 1, 3});

        sortMagicCards(v, MagicSortColumn::Foil, /*ascending=*/false);
        CHECK(ids(v) == std::vector<std::uint32_t>{1, 3, 2, 4});
    }

    TEST_CASE("Signed and Altered booleans sort independently") {
        std::vector<MagicCard> v = {
            mc(1, "a", "X", "2000/01/01", 1, false, /*sgnd=*/false, /*alt=*/true),
            mc(2, "b", "X", "2000/01/01", 1, false, /*sgnd=*/true,  /*alt=*/false),
        };
        sortMagicCards(v, MagicSortColumn::Signed, /*ascending=*/true);
        CHECK(ids(v) == std::vector<std::uint32_t>{1, 2});

        sortMagicCards(v, MagicSortColumn::Altered, /*ascending=*/true);
        CHECK(ids(v) == std::vector<std::uint32_t>{2, 1});
    }

    TEST_CASE("Language and Condition sort by their string label, lowercased") {
        std::vector<MagicCard> v = {
            mc(1, "a", "X", "2000/01/01", 1, false, false, false,
               Language::Japanese, Condition::Mint),
            mc(2, "b", "X", "2000/01/01", 1, false, false, false,
               Language::English, Condition::Played),
            mc(3, "c", "X", "2000/01/01", 1, false, false, false,
               Language::German, Condition::NearMint),
        };
        sortMagicCards(v, MagicSortColumn::Language, /*ascending=*/true);
        // english < german < japanese (lowercased compare)
        CHECK(ids(v) == std::vector<std::uint32_t>{2, 3, 1});

        sortMagicCards(v, MagicSortColumn::Condition, /*ascending=*/true);
        // mint < nearmint < played (lowercased compare)
        CHECK(ids(v) == std::vector<std::uint32_t>{1, 3, 2});
    }

    TEST_CASE("Note sorts case-insensitively") {
        std::vector<MagicCard> v = {
            mc(1, "a", "X", "2000/01/01", 1, false, false, false,
               Language::English, Condition::NearMint, "Zeta"),
            mc(2, "b", "X", "2000/01/01", 1, false, false, false,
               Language::English, Condition::NearMint, "alpha"),
            mc(3, "c", "X", "2000/01/01", 1, false, false, false,
               Language::English, Condition::NearMint, "Beta"),
        };
        sortMagicCards(v, MagicSortColumn::Note, /*ascending=*/true);
        CHECK(ids(v) == std::vector<std::uint32_t>{2, 3, 1});  // alpha, beta, zeta
    }

    TEST_CASE("stable: sort by name then by set keeps name order within each set") {
        // Mirrors the UX expectation: a user clicks Name, then Set, and
        // sees rows grouped by set with names alphabetical inside each group.
        std::vector<MagicCard> v = {
            mc(1, "Counterspell", "Beta",  "1993/10/04"),
            mc(2, "Brainstorm",   "Alpha", "1993/08/05"),
            mc(3, "Lightning Bolt","Beta", "1993/10/04"),
            mc(4, "Ancestral Recall","Alpha","1993/08/05"),
        };
        sortMagicCards(v, MagicSortColumn::Name,           /*asc=*/true);
        sortMagicCards(v, MagicSortColumn::SetReleaseDate, /*asc=*/true);
        // Alpha (1993/08/05) first: ancestral, brainstorm
        // Beta  (1993/10/04) next:  counterspell, lightning bolt
        CHECK(ids(v) == std::vector<std::uint32_t>{4, 2, 1, 3});
    }
}

TEST_SUITE("CardSorter - Pokemon-specific columns") {
    TEST_CASE("Holo and FirstEdition each sort their own bool field") {
        std::vector<PokemonCard> v = {
            pc(1, "a", "X", "2000/01/01", 1, /*holo=*/true,  /*1st=*/false),
            pc(2, "b", "X", "2000/01/01", 1, /*holo=*/false, /*1st=*/true),
            pc(3, "c", "X", "2000/01/01", 1, /*holo=*/false, /*1st=*/false),
        };
        sortPokemonCards(v, PokemonSortColumn::Holo, /*ascending=*/true);
        CHECK(ids(v) == std::vector<std::uint32_t>{2, 3, 1});

        sortPokemonCards(v, PokemonSortColumn::FirstEdition, /*ascending=*/true);
        // After previous sort: 2,3,1 (false=2, false=1 actually... let me think
        // -- with v in order {2,3,1} their firstEdition = {true,false,false}.
        // Stable asc on firstEdition keeps 3 before 1 in the false bucket.)
        CHECK(ids(v) == std::vector<std::uint32_t>{3, 1, 2});
    }

    TEST_CASE("Set column sorts by release date for Pokemon too") {
        std::vector<PokemonCard> v = {
            pc(1, "x", "Sun & Moon",   "2017/02/03"),
            pc(2, "y", "Base Set",     "1999/01/09"),
            pc(3, "z", "Sword & Shield","2020/02/07"),
        };
        sortPokemonCards(v, PokemonSortColumn::SetReleaseDate, /*ascending=*/true);
        CHECK(ids(v) == std::vector<std::uint32_t>{2, 1, 3});
    }

    TEST_CASE("Amount sorts numerically") {
        std::vector<PokemonCard> v = {
            pc(1, "a", "X", "2000/01/01", 9),
            pc(2, "b", "X", "2000/01/01", 11),
            pc(3, "c", "X", "2000/01/01", 1),
        };
        sortPokemonCards(v, PokemonSortColumn::Amount, /*ascending=*/true);
        CHECK(ids(v) == std::vector<std::uint32_t>{3, 1, 2});
    }
}

TEST_SUITE("CardSorter - empty / single-element inputs are no-ops") {
    TEST_CASE("empty vector stays empty") {
        std::vector<MagicCard> v;
        sortMagicCards(v, MagicSortColumn::Name, true);
        CHECK(v.empty());
    }

    TEST_CASE("single element preserved") {
        std::vector<MagicCard> v = { mc(42, "Solo", "X", "2000/01/01") };
        sortMagicCards(v, MagicSortColumn::Amount, false);
        CHECK(v.size() == 1);
        CHECK(v.front().id == 42);
    }
}

TEST_SUITE("CardSorter - YuGiOh columns") {
    TEST_CASE("Set # and Rarity sort lexicographically") {
        std::vector<YuGiOhCard> v = {
            yc(1, "A", "S1", "2000/01/01", "SDK-002", "Common"),
            yc(2, "B", "S1", "2000/01/01", "SDK-001", "Ultra Rare"),
            yc(3, "C", "S1", "2000/01/01", "SDK-003", "Secret Rare"),
        };
        sortYuGiOhCards(v, YuGiOhSortColumn::SetNo, true);
        CHECK(ids(v) == std::vector<std::uint32_t>{2, 1, 3});

        sortYuGiOhCards(v, YuGiOhSortColumn::Rarity, true);
        CHECK(ids(v) == std::vector<std::uint32_t>{1, 3, 2});
    }
}
