#include <doctest/doctest.h>

#include "ccm/util/CardLookupDetect.hpp"

using namespace ccm;

TEST_SUITE("preferDetectBySetNo") {
    TEST_CASE("only set number filled uses reverse lookup") {
        CHECK(preferDetectBySetNo(true, false, CardLookupEditField::None));
        CHECK(preferDetectBySetNo(true, false, CardLookupEditField::Name));
        CHECK(preferDetectBySetNo(true, false, CardLookupEditField::SetNo));
    }

    TEST_CASE("only name filled uses name lookup") {
        CHECK_FALSE(preferDetectBySetNo(false, true, CardLookupEditField::None));
        CHECK_FALSE(preferDetectBySetNo(false, true, CardLookupEditField::Name));
        CHECK_FALSE(preferDetectBySetNo(false, true, CardLookupEditField::SetNo));
    }

    TEST_CASE("both empty does not prefer reverse") {
        CHECK_FALSE(preferDetectBySetNo(true, true, CardLookupEditField::None));
        CHECK_FALSE(preferDetectBySetNo(true, true, CardLookupEditField::SetNo));
    }

    TEST_CASE("both filled: last edited SetNo prefers reverse") {
        CHECK(preferDetectBySetNo(false, false, CardLookupEditField::SetNo));
    }

    TEST_CASE("both filled: Name or None keeps name lookup") {
        CHECK_FALSE(preferDetectBySetNo(false, false, CardLookupEditField::Name));
        CHECK_FALSE(preferDetectBySetNo(false, false, CardLookupEditField::None));
    }
}
