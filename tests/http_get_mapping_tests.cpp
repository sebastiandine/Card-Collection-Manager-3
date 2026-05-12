#include <doctest/doctest.h>

#include "ccm/util/HttpGetMapping.hpp"

using namespace ccm;

TEST_SUITE("mapHttpGetResponse") {
    TEST_CASE("curl transport error ignores HTTP status and body") {
        const auto out =
            mapHttpGetResponse(true, "connection refused", 0, "ignored", "http://x");
        REQUIRE(out.isErr());
        CHECK(out.error() == "HTTP error: connection refused");
    }

    TEST_CASE("HTTP status below 200 is an error") {
        const auto out =
            mapHttpGetResponse(false, {}, 199, "body", "http://example/a");
        REQUIRE(out.isErr());
        CHECK(out.error() == "HTTP 199 from http://example/a");
    }

    TEST_CASE("HTTP status 200 returns body") {
        const auto out =
            mapHttpGetResponse(false, {}, 200, "payload", "http://example/a");
        REQUIRE(out.isOk());
        CHECK(out.value() == "payload");
    }

    TEST_CASE("HTTP status 299 returns body") {
        const auto out =
            mapHttpGetResponse(false, {}, 299, "ok", "http://example/a");
        REQUIRE(out.isOk());
        CHECK(out.value() == "ok");
    }

    TEST_CASE("HTTP status 300 and above is an error") {
        const auto out =
            mapHttpGetResponse(false, {}, 300, "redirect", "http://example/a");
        REQUIRE(out.isErr());
        CHECK(out.error() == "HTTP 300 from http://example/a");
    }

    TEST_CASE("HTTP 404 formats URL into message") {
        const auto out =
            mapHttpGetResponse(false, {}, 404, "", "https://api.example/r");
        REQUIRE(out.isErr());
        CHECK(out.error() == "HTTP 404 from https://api.example/r");
    }

    TEST_CASE("HTTP 200 with empty body still maps to success") {
        const auto out =
            mapHttpGetResponse(false, {}, 200, "", "https://api.example/empty");
        REQUIRE(out.isOk());
        CHECK(out.value().empty());
    }
}
