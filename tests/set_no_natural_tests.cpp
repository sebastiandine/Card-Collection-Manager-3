#include <doctest/doctest.h>

#include "ccm/util/SetNoNatural.hpp"

#include <algorithm>
#include <string>
#include <vector>

using ccm::compareSetNoNatural;

TEST_SUITE("compareSetNoNatural") {
    TEST_CASE("orders pure digits numerically") {
        CHECK(compareSetNoNatural("1", "2") < 0);
        CHECK(compareSetNoNatural("2", "10") < 0);
        CHECK(compareSetNoNatural("10", "100") < 0);
        CHECK(compareSetNoNatural("2", "1") > 0);
        CHECK(compareSetNoNatural("10", "2") > 0);
    }

    TEST_CASE("leading zeros tie numerically then lex") {
        CHECK(compareSetNoNatural("001", "1") != 0);
        CHECK(compareSetNoNatural("1", "001") > 0);  // "001" < "1" lexicographically
        CHECK(compareSetNoNatural("001", "002") < 0);
        CHECK(compareSetNoNatural("001", "001") == 0);
    }

    TEST_CASE("alpha prefix then numeric run") {
        CHECK(compareSetNoNatural("SWSH001", "SWSH002") < 0);
        CHECK(compareSetNoNatural("SWSH10", "SWSH2") > 0);
        CHECK(compareSetNoNatural("A10", "B2") < 0);
    }

    TEST_CASE("sorts base-set style list into numeric order") {
        std::vector<std::string> nos{"1", "10", "100", "2", "20", "3"};
        std::sort(nos.begin(), nos.end(), [](const std::string& a, const std::string& b) {
            return compareSetNoNatural(a, b) < 0;
        });
        CHECK(nos == std::vector<std::string>{"1", "2", "3", "10", "20", "100"});
    }
}
