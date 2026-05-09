#include <doctest/doctest.h>

#include "ccm/util/FsNames.hpp"

using ccm::formatTextForFs;
using ccm::parseIndexFromFilename;

TEST_SUITE("FsNames::formatTextForFs") {
    TEST_CASE("strips spaces, commas, apostrophes, backticks") {
        CHECK(formatTextForFs("Hello, World's Set") == "HelloWorldsSet");
        CHECK(formatTextForFs("`tick`") == "tick");
    }

    TEST_CASE("colons become hyphens") {
        CHECK(formatTextForFs("Set: Subtitle") == "Set-Subtitle");
    }

    TEST_CASE("ampersand becomes And, pipe becomes Or") {
        CHECK(formatTextForFs("Black & White") == "BlackAndWhite");
        CHECK(formatTextForFs("a|b") == "aOrb");
    }

    TEST_CASE("flattens accented vowels listed in the original Rust source") {
        // UTF-8 sequences for the accented characters the Rust crate handles.
        CHECK(formatTextForFs("\xC3\xA1\xC3\xA9\xC3\xAD\xC3\xB3\xC3\xBA\xC3\xBB") == "aeiouu");
    }

    TEST_CASE("idempotent on already-clean strings") {
        CHECK(formatTextForFs("AlreadyClean") == "AlreadyClean");
    }
}

TEST_SUITE("FsNames::parseIndexFromFilename") {
    TEST_CASE("single digit") {
        CHECK(parseIndexFromFilename("Image1.png")  == 1);
        CHECK(parseIndexFromFilename("foo+bar+0.jpg") == 0);
    }

    TEST_CASE("two digit") {
        CHECK(parseIndexFromFilename("Image22.jpeg") == 22);
        CHECK(parseIndexFromFilename("set+name+99.png") == 99);
    }

    TEST_CASE("only the last two digits are taken (matches Rust source)") {
        CHECK(parseIndexFromFilename("foo+123+45.png") == 45);
    }

    TEST_CASE("no digits or no extension returns 0") {
        CHECK(parseIndexFromFilename("noindex.png") == 0);
        CHECK(parseIndexFromFilename("nothing")     == 0);
        CHECK(parseIndexFromFilename("")            == 0);
    }
}
