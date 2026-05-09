#include <doctest/doctest.h>

#include "ccm/games/magic/MagicSetSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

using namespace ccm;

namespace {

class FixedHttpClient final : public IHttpClient {
public:
    std::string body;
    bool ok = true;
    Result<std::string> get(std::string_view) override {
        return ok ? Result<std::string>::ok(body)
                  : Result<std::string>::err("offline");
    }
};

}  // namespace

TEST_SUITE("MagicSetSource::parseResponse") {
    TEST_CASE("filters digital sets and converts release date format") {
        const std::string json = R"({
            "data": [
                {"code":"lea","name":"Alpha","released_at":"1993-08-05","digital":false},
                {"code":"mtgo","name":"Online Promo","released_at":"2010-01-01","digital":true},
                {"code":"leb","name":"Beta","released_at":"1993-10-04","digital":false}
            ]
        })";

        const auto out = MagicSetSource::parseResponse(json);
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(out.value()[0].id == "lea");
        CHECK(out.value()[0].releaseDate == "1993/08/05");
        CHECK(out.value()[1].id == "leb");
        CHECK(out.value()[1].releaseDate == "1993/10/04");
    }

    TEST_CASE("sorts by release date ascending") {
        const std::string json = R"({
            "data": [
                {"code":"newer","name":"N","released_at":"2024-01-01","digital":false},
                {"code":"older","name":"O","released_at":"2010-01-01","digital":false}
            ]
        })";
        const auto out = MagicSetSource::parseResponse(json);
        REQUIRE(out.isOk());
        CHECK(out.value().front().id == "older");
        CHECK(out.value().back().id  == "newer");
    }

    TEST_CASE("missing data array returns an error") {
        const auto out = MagicSetSource::parseResponse(R"({"meta":{}})");
        CHECK(out.isErr());
    }

    TEST_CASE("invalid JSON returns an error") {
        const auto out = MagicSetSource::parseResponse("{not json");
        CHECK(out.isErr());
    }
}

TEST_SUITE("MagicSetSource::fetchAll") {
    TEST_CASE("network error is surfaced as a Result error") {
        FixedHttpClient http;
        http.ok = false;
        MagicSetSource src{http};
        CHECK(src.fetchAll().isErr());
    }

    TEST_CASE("network success is parsed end-to-end") {
        FixedHttpClient http;
        http.ok = true;
        http.body = R"({"data":[{"code":"x","name":"X","released_at":"2020-01-01","digital":false}]})";
        MagicSetSource src{http};
        const auto out = src.fetchAll();
        REQUIRE(out.isOk());
        CHECK(out.value().front().id == "x");
        CHECK(out.value().front().releaseDate == "2020/01/01");
    }
}
