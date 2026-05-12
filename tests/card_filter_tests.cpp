// CardFilter tests - exercises the per-row matcher ported from the table
// TableTemplate.tsx::applyFilter. Each TEST_CASE pins down a behavior the
// original UI relied on, so a regression here implies the C++ table no
// longer filters like the TypeScript reference.

#include <doctest/doctest.h>

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/MagicCard.hpp"
#include "ccm/domain/PokemonCard.hpp"
#include "ccm/domain/YuGiOhCard.hpp"
#include "ccm/services/CardFilter.hpp"

#include <string>

using namespace ccm;

namespace {

MagicCard mc(std::string name,
             std::string setName,
             std::uint8_t amount = 1,
             Language lang = Language::English,
             Condition cond = Condition::NearMint,
             std::string note = "",
             bool foil = false, bool sgnd = false, bool altered = false) {
    MagicCard c;
    c.id = 1;
    c.name = std::move(name);
    c.set.name = std::move(setName);
    c.amount = amount;
    c.language = lang;
    c.condition = cond;
    c.note = std::move(note);
    c.foil = foil;
    c.signed_ = sgnd;
    c.altered = altered;
    return c;
}

PokemonCard pc(std::string name,
               std::string setName,
               std::string setNo = "",
               std::uint8_t amount = 1,
               std::string note = "") {
    PokemonCard c;
    c.id = 1;
    c.name = std::move(name);
    c.set.name = std::move(setName);
    c.setNo = std::move(setNo);
    c.amount = amount;
    c.note = std::move(note);
    return c;
}

YuGiOhCard yc(std::string name,
              std::string setName,
              std::string setNo = "",
              std::string rarity = "",
              std::uint8_t amount = 1,
              Language lang = Language::English,
              Condition cond = Condition::NearMint,
              std::string note = "",
              bool firstEdition = false,
              bool sgnd = false,
              bool altered = false) {
    YuGiOhCard c;
    c.id = 1;
    c.name = std::move(name);
    c.set.name = std::move(setName);
    c.setNo = std::move(setNo);
    c.rarity = std::move(rarity);
    c.amount = amount;
    c.language = lang;
    c.condition = cond;
    c.note = std::move(note);
    c.firstEdition = firstEdition;
    c.signed_ = sgnd;
    c.altered = altered;
    return c;
}

}  // namespace

TEST_SUITE("CardFilter::matchesMagicFilter") {
    TEST_CASE("empty filter matches every row (mirrors JS \"\".includes(\"\"))") {
        CHECK(matchesMagicFilter(mc("Brainstorm", "Alpha"), ""));
    }

    TEST_CASE("matches by name (case-insensitive substring)") {
        const MagicCard c = mc("Lightning Bolt", "Beta");
        CHECK(matchesMagicFilter(c, "lightning"));
        CHECK(matchesMagicFilter(c, "BOLT"));     // both sides lowercased
        CHECK(matchesMagicFilter(c, "ning Bo"));  // mid-string substring
        CHECK_FALSE(matchesMagicFilter(c, "fireball"));
    }

    TEST_CASE("matches by set.name (valueKey 'set.name', not 'set')") {
        const MagicCard c = mc("Brainstorm", "Modern Horizons");
        CHECK(matchesMagicFilter(c, "Modern"));
        CHECK(matchesMagicFilter(c, "horizons"));
        CHECK_FALSE(matchesMagicFilter(c, "Zendikar"));
    }

    TEST_CASE("matches by language label") {
        const MagicCard c = mc("Brainstorm", "Alpha", 1, Language::Japanese);
        CHECK(matchesMagicFilter(c, "japanese"));
        CHECK_FALSE(matchesMagicFilter(c, "german"));
    }

    TEST_CASE("matches by condition label") {
        const MagicCard c = mc("Brainstorm", "Alpha", 1,
                               Language::English, Condition::Played);
        CHECK(matchesMagicFilter(c, "played"));
        CHECK_FALSE(matchesMagicFilter(c, "mint"));
    }

    TEST_CASE("matches by amount as decimal string (val.toString())") {
        // JS used `val.toString().toLowerCase().includes(filter)` so the
        // *integer* amount becomes searchable as its decimal representation.
        const MagicCard c = mc("Brainstorm", "Alpha", 17);
        CHECK(matchesMagicFilter(c, "17"));
        CHECK(matchesMagicFilter(c, "1"));   // substring match: "1" in "17"
        CHECK_FALSE(matchesMagicFilter(c, "99"));
    }

    TEST_CASE("matches by note (case-insensitive)") {
        const MagicCard c = mc("Brainstorm", "Alpha", 1,
                               Language::English, Condition::NearMint,
                               "Birthday gift");
        CHECK(matchesMagicFilter(c, "Birthday"));
        CHECK(matchesMagicFilter(c, "GIFT"));
        CHECK_FALSE(matchesMagicFilter(c, "trade"));
    }

    TEST_CASE("boolean flag columns (foil / signed / altered) are NOT matched") {
        // Filtering checks only string- or number-typed cells (typeof check). The
        // bool flag columns therefore must not match the literal "true" /
        // "false" — even when the flag is set.
        const MagicCard c = mc("Brainstorm", "Alpha", 1,
                               Language::English, Condition::NearMint, "",
                               /*foil=*/true, /*sgnd=*/true, /*altered=*/true);
        CHECK_FALSE(matchesMagicFilter(c, "true"));
        CHECK_FALSE(matchesMagicFilter(c, "false"));
        // ...unless the literal happens to be a substring of an actual
        // string-typed column. Sanity-check that the matcher still fires on
        // the name field with the same haystack.
        CHECK(matchesMagicFilter(c, "brain"));
    }

    TEST_CASE("filter is lowercased so uppercase input matches lowercased data") {
        // The original path only lowercased the cell value, so "Brainstorm" filter against
        // a card named "brainstorm" never matched. We lowercase both sides;
        // pin that down here so a regression to the literal JS behavior gets
        // caught.
        const MagicCard c = mc("brainstorm", "alpha");
        CHECK(matchesMagicFilter(c, "BRAIN"));
        CHECK(matchesMagicFilter(c, "Alpha"));
    }
}

TEST_SUITE("CardFilter::matchesPokemonFilter") {
    TEST_CASE("matches by name and set.name") {
        const PokemonCard c = pc("Charizard", "Base Set");
        CHECK(matchesPokemonFilter(c, "char"));
        CHECK(matchesPokemonFilter(c, "BASE"));
        CHECK_FALSE(matchesPokemonFilter(c, "pikachu"));
    }

    TEST_CASE("Pokemon adds setNo to the searchable columns") {
        // PokemonTable.tsx tableFields includes a "Set No" column whose
        // valueKey is `setNo`. Pin that down — `setNo` is Pokemon-specific so
        // the Magic matcher does not see it.
        const PokemonCard c = pc("Charizard", "Base Set", "4/102");
        CHECK(matchesPokemonFilter(c, "4/102"));
        CHECK(matchesPokemonFilter(c, "/102"));
    }

    TEST_CASE("amount is searchable as decimal string for Pokemon too") {
        const PokemonCard c = pc("Charizard", "Base Set", "4/102", 23);
        CHECK(matchesPokemonFilter(c, "23"));
        CHECK_FALSE(matchesPokemonFilter(c, "99"));
    }

    TEST_CASE("empty filter matches everything") {
        CHECK(matchesPokemonFilter(pc("Charizard", "Base Set"), ""));
    }
}

TEST_SUITE("CardFilter::matchesYuGiOhFilter") {
    TEST_CASE("empty filter matches every row") {
        CHECK(matchesYuGiOhFilter(yc("Dark Magician", "Legend of Blue Eyes"), ""));
    }

    TEST_CASE("matches by set number and rarity") {
        const YuGiOhCard c = yc("Dark Magician", "Legend of Blue Eyes", "LOB-005", "Ultra Rare");
        CHECK(matchesYuGiOhFilter(c, "lob-005"));
        CHECK(matchesYuGiOhFilter(c, "ultra"));
        CHECK(matchesYuGiOhFilter(c, "ur"));
        CHECK_FALSE(matchesYuGiOhFilter(c, "secret rare"));
    }

    TEST_CASE("matches by name and set.name") {
        const YuGiOhCard c = yc("Dark Magician", "Legend of Blue Eyes", "LOB-005", "Ultra Rare");
        CHECK(matchesYuGiOhFilter(c, "dark"));
        CHECK(matchesYuGiOhFilter(c, "blue eyes"));
        CHECK_FALSE(matchesYuGiOhFilter(c, "spell"));
    }

    TEST_CASE("matches by language, condition, amount, and note") {
        const YuGiOhCard c = yc("Dark Magician", "Legend of Blue Eyes", "LOB-005", "Ultra Rare",
                                 12, Language::German, Condition::Played, "binder copy");
        CHECK(matchesYuGiOhFilter(c, "german"));
        CHECK(matchesYuGiOhFilter(c, "played"));
        CHECK(matchesYuGiOhFilter(c, "12"));
        CHECK(matchesYuGiOhFilter(c, "binder"));
        CHECK_FALSE(matchesYuGiOhFilter(c, "english"));
    }

    TEST_CASE("matches rarity shorthand when the long rarity string does not") {
        const YuGiOhCard c = yc("Dark Magician", "Legend of Blue Eyes", "LOB-005",
                                "Quarter Century Secret Rare");
        CHECK(matchesYuGiOhFilter(c, "qcscr"));
        CHECK_FALSE(matchesYuGiOhFilter(c, "mythic"));
    }

    TEST_CASE("boolean flag columns are not matched") {
        const YuGiOhCard c = yc("Dark Magician", "Legend of Blue Eyes", "LOB-005", "Ultra Rare",
                                 1, Language::English, Condition::NearMint, "",
                                 /*firstEdition=*/true, /*sgnd=*/true, /*altered=*/true);
        CHECK_FALSE(matchesYuGiOhFilter(c, "true"));
        CHECK_FALSE(matchesYuGiOhFilter(c, "false"));
        CHECK(matchesYuGiOhFilter(c, "dark"));
    }

    TEST_CASE("no column hit returns false") {
        const YuGiOhCard c = yc("Dark Magician", "Legend of Blue Eyes", "LOB-005", "Ultra Rare");
        CHECK_FALSE(matchesYuGiOhFilter(c, "zzznomatch"));
    }
}
