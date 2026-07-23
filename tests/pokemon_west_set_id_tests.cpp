#include <doctest/doctest.h>

#include "ccm/games/pokemon/PokemonWestSetId.hpp"

using namespace ccm;

TEST_SUITE("canonicalizeWestSetId") {
    TEST_CASE("identity for ids already on TCGdex") {
        CHECK(canonicalizeWestSetId("base1") == "base1");
        CHECK(canonicalizeWestSetId("swsh3") == "swsh3");
        CHECK(canonicalizeWestSetId("sv01") == "sv01");
        CHECK(canonicalizeWestSetId("sv10") == "sv10");
    }

    TEST_CASE("maps Scarlet & Violet divergences") {
        CHECK(canonicalizeWestSetId("sv1") == "sv01");
        CHECK(canonicalizeWestSetId("sv3") == "sv03");
        CHECK(canonicalizeWestSetId("sv3pt5") == "sv03.5");
        CHECK(canonicalizeWestSetId("sv8pt5") == "sv08.5");
        CHECK(canonicalizeWestSetId("zsv10pt5") == "sv10.5b");
        CHECK(canonicalizeWestSetId("rsv10pt5") == "sv10.5w");
    }

    TEST_CASE("maps SWSH galleries and specials") {
        CHECK(canonicalizeWestSetId("pgo") == "swsh10.5");
        CHECK(canonicalizeWestSetId("swsh12tg") == "swsh12.5tg");
        CHECK(canonicalizeWestSetId("swsh12pt5") == "swsh12.5");
        CHECK(canonicalizeWestSetId("swsh45") == "swsh4.5");
        CHECK(canonicalizeWestSetId("cel25c") == "cel25cc");
    }

    TEST_CASE("maps McDonald's year codes") {
        CHECK(canonicalizeWestSetId("mcd19") == "2019sm");
        CHECK(canonicalizeWestSetId("mcd22") == "2022swsh");
    }

    TEST_CASE("unknown and empty pass through") {
        CHECK(canonicalizeWestSetId("fut20") == "fut20");
        CHECK(canonicalizeWestSetId("").empty());
    }
}
