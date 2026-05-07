#include <doctest/doctest.h>

#include "ccm/games/pokemon/PokemonSetSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

using namespace ccm;

namespace {

class FixedHttpClient final : public IHttpClient {
public:
    std::string lastUrl;
    std::string body;
    bool ok = true;
    Result<std::string> get(std::string_view url) override {
        lastUrl = std::string(url);
        return ok ? Result<std::string>::ok(body)
                  : Result<std::string>::err("offline");
    }
};

}  // namespace

TEST_SUITE("PokemonSetSource::parseResponse") {
    TEST_CASE("happy path: maps id/name/releaseDate without rewriting separators") {
        // The Pokemon TCG API returns releaseDate already in YYYY/MM/DD form,
        // unlike Scryfall's released_at YYYY-MM-DD.
        const std::string json = R"({
            "data": [
                {"id":"base1","name":"Base","releaseDate":"1999/01/09"},
                {"id":"jungle","name":"Jungle","releaseDate":"1999/06/16"}
            ]
        })";

        const auto out = PokemonSetSource::parseResponse(json);
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(out.value()[0].id == "base1");
        CHECK(out.value()[0].name == "Base");
        CHECK(out.value()[0].releaseDate == "1999/01/09");
        CHECK(out.value()[1].id == "jungle");
        CHECK(out.value()[1].releaseDate == "1999/06/16");
    }

    TEST_CASE("sorts by release date ascending") {
        const std::string json = R"({
            "data": [
                {"id":"newer","name":"N","releaseDate":"2024/01/01"},
                {"id":"older","name":"O","releaseDate":"2010/01/01"}
            ]
        })";
        const auto out = PokemonSetSource::parseResponse(json);
        REQUIRE(out.isOk());
        CHECK(out.value().front().id == "older");
        CHECK(out.value().back().id  == "newer");
    }

    TEST_CASE("empty data array returns an empty list (not an error)") {
        const auto out = PokemonSetSource::parseResponse(R"({"data":[]})");
        REQUIRE(out.isOk());
        CHECK(out.value().empty());
    }

    TEST_CASE("missing data array returns an error") {
        const auto out = PokemonSetSource::parseResponse(R"({"meta":{}})");
        CHECK(out.isErr());
    }

    TEST_CASE("invalid JSON returns an error") {
        const auto out = PokemonSetSource::parseResponse("{not json");
        CHECK(out.isErr());
    }
}

TEST_SUITE("PokemonSetSource::fetchAll") {
    TEST_CASE("network error is surfaced as a Result error") {
        FixedHttpClient http;
        http.ok = false;
        PokemonSetSource src{http};
        CHECK(src.fetchAll().isErr());
    }

    TEST_CASE("network success is parsed end-to-end and hits the public endpoint") {
        FixedHttpClient http;
        http.ok = true;
        http.body = R"({"data":[{"id":"x","name":"X","releaseDate":"2020/01/01"}]})";
        PokemonSetSource src{http};
        const auto out = src.fetchAll();
        REQUIRE(out.isOk());
        CHECK(out.value().front().id == "x");
        CHECK(out.value().front().releaseDate == "2020/01/01");
        CHECK(http.lastUrl == "https://api.pokemontcg.io/v2/sets");
    }
}
