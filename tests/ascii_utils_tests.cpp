#include <doctest/doctest.h>

#include "ccm/util/AsciiUtils.hpp"

using namespace ccm;

TEST_SUITE("asciiLower") {
    TEST_CASE("empty string stays empty") {
        CHECK(asciiLower("").empty());
    }

    TEST_CASE("lowercases ASCII letters and leaves other ASCII bytes unchanged") {
        CHECK(asciiLower("AbC123!@#") == "abc123!@#");
    }

    TEST_CASE("non-ASCII UTF-8 bytes pass through unchanged") {
        const std::string input = "caf\u00e9";
        CHECK(asciiLower(input) == input);
    }
}
