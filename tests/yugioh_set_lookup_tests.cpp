#include <doctest/doctest.h>

#include "ccm/domain/Set.hpp"
#include "ccm/util/YuGiOhSetLookup.hpp"

using namespace ccm;

namespace {

std::vector<Set> sampleSets() {
    return {
        Set{.id = "LOB", .name = "Legend of Blue Eyes White Dragon", .releaseDate = "2002/03/08"},
        Set{.id = "MRD", .name = "Metal Raiders", .releaseDate = "2002/06/26"},
        Set{.id = "LOB-25TH", .name = "Legend of Blue Eyes White Dragon (25th Anniversary Edition)",
             .releaseDate = "2023/04/20"},
    };
}

}  // namespace

TEST_SUITE("lookupYuGiOhSetByShorthand") {
    using Kind = YuGiOhSetShorthandLookup::Kind;

    TEST_CASE("empty and whitespace-only query is NotFound") {
        const auto sets = sampleSets();
        CHECK(lookupYuGiOhSetByShorthand("", sets).kind == Kind::NotFound);
        CHECK(lookupYuGiOhSetByShorthand("   ", sets).kind == Kind::NotFound);
        CHECK(lookupYuGiOhSetByShorthand("\t\n", sets).kind == Kind::NotFound);
    }

    TEST_CASE("case-insensitive exact id match is Unique") {
        const auto sets = sampleSets();
        auto r = lookupYuGiOhSetByShorthand("lob", sets);
        REQUIRE(r.kind == Kind::Unique);
        CHECK(r.index == 0);
        CHECK(sets[r.index].id == "LOB");

        r = lookupYuGiOhSetByShorthand("MRD", sets);
        REQUIRE(r.kind == Kind::Unique);
        CHECK(r.index == 1);
    }

    TEST_CASE("trim ASCII whitespace around query") {
        const auto sets = sampleSets();
        const auto r = lookupYuGiOhSetByShorthand("  LOB  ", sets);
        REQUIRE(r.kind == Kind::Unique);
        CHECK(r.index == 0);
    }

    TEST_CASE("hyphenated set codes match") {
        const auto sets = sampleSets();
        const auto r = lookupYuGiOhSetByShorthand("lob-25th", sets);
        REQUIRE(r.kind == Kind::Unique);
        CHECK(r.index == 2);
        CHECK(sets[r.index].id == "LOB-25TH");
    }

    TEST_CASE("unknown code is NotFound") {
        const auto sets = sampleSets();
        CHECK(lookupYuGiOhSetByShorthand("NOPE", sets).kind == Kind::NotFound);
    }

    TEST_CASE("Ambiguous when two sets share the same normalized id") {
        std::vector<Set> dup = {
            Set{.id = "X1", .name = "A", .releaseDate = "2000/01/01"},
            Set{.id = "x1", .name = "B", .releaseDate = "2000/01/02"},
        };
        CHECK(lookupYuGiOhSetByShorthand("X1", dup).kind == Kind::Ambiguous);
    }

    TEST_CASE("first matching index is stable when Unique among similar prefixes") {
        const auto sets = sampleSets();
        const auto r = lookupYuGiOhSetByShorthand("LOB", sets);
        REQUIRE(r.kind == Kind::Unique);
        CHECK(r.index == 0);
        CHECK(sets[r.index].id == "LOB");
    }

    TEST_CASE("normalizeYuGiOhSetIdForLookup lowercases ASCII") {
        CHECK(normalizeYuGiOhSetIdForLookup("Ra04-EN001") == "ra04-en001");
    }

    TEST_CASE("trimAsciiWhitespace handles empty") {
        CHECK(trimAsciiWhitespace("") == "");
        CHECK(trimAsciiWhitespace("x") == "x");
    }
}
