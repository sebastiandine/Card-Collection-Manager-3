#include <doctest/doctest.h>

#include "ccm/games/pokemon/PokemonCardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>

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

TEST_SUITE("PokemonCardPreviewSource::buildSearchUrl") {
    TEST_CASE("name and setId produce a percent-encoded query") {
        const auto url = PokemonCardPreviewSource::buildSearchUrl(
            "Pikachu", "base1", "");
        CHECK(url.find("https://api.pokemontcg.io/v2/cards?q=") == 0);
        CHECK(url.find("%22Pikachu%22") != std::string::npos);
        CHECK(url.find("set.id%3Abase1") != std::string::npos);
        // No number term when setNo is empty.
        CHECK(url.find("number") == std::string::npos);
    }

    TEST_CASE("setNo is appended as a number: clause") {
        const auto url = PokemonCardPreviewSource::buildSearchUrl(
            "Charizard", "base1", "4");
        CHECK(url.find("number%3A4") != std::string::npos);
    }

    TEST_CASE("setNo with a slash is normalized to the printed number") {
        // Pokemon collection numbers are commonly stored as "4/102" — the
        // Pokemon TCG search API only accepts the printed-number portion.
        const auto url = PokemonCardPreviewSource::buildSearchUrl(
            "Charizard", "base1", "4/102");
        CHECK(url.find("number%3A4") != std::string::npos);
        CHECK(url.find("102") == std::string::npos);
    }

    TEST_CASE("name with spaces is percent-encoded") {
        const auto url = PokemonCardPreviewSource::buildSearchUrl(
            "Mr. Mime", "base1", "");
        CHECK(url.find("%22Mr.%20Mime%22") != std::string::npos);
    }
}

TEST_SUITE("PokemonCardPreviewSource::parseResponse") {
    TEST_CASE("returns images.large when present") {
        const std::string json = R"({
            "data": [
                {
                    "name": "Pikachu",
                    "images": {
                        "small": "https://images.pokemontcg.io/small.png",
                        "large": "https://images.pokemontcg.io/large.png"
                    }
                }
            ]
        })";
        const auto out = PokemonCardPreviewSource::parseResponse(json);
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://images.pokemontcg.io/large.png");
    }

    TEST_CASE("falls back to images.small when large is absent") {
        const std::string json = R"({
            "data": [
                {"name":"Pikachu","images":{"small":"https://small.only/img.png"}}
            ]
        })";
        const auto out = PokemonCardPreviewSource::parseResponse(json);
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://small.only/img.png");
    }

    TEST_CASE("empty data array returns an error") {
        const auto out = PokemonCardPreviewSource::parseResponse(R"({"data":[]})");
        CHECK(out.isErr());
    }

    TEST_CASE("missing data array returns an error") {
        const auto out = PokemonCardPreviewSource::parseResponse(R"({"meta":{}})");
        CHECK(out.isErr());
    }

    TEST_CASE("entry without images returns an error") {
        const auto out = PokemonCardPreviewSource::parseResponse(
            R"({"data":[{"name":"Pikachu"}]})");
        CHECK(out.isErr());
    }

    TEST_CASE("invalid JSON returns an error") {
        const auto out = PokemonCardPreviewSource::parseResponse("{not json");
        CHECK(out.isErr());
    }
}

TEST_SUITE("PokemonCardPreviewSource::fetchImageUrl") {
    TEST_CASE("network error is surfaced as a Result error") {
        FixedHttpClient http;
        http.ok = false;
        PokemonCardPreviewSource src{http};
        CHECK(src.fetchImageUrl("Pikachu", "base1", "").isErr());
    }

    TEST_CASE("network success is parsed end-to-end and uses the encoded URL") {
        FixedHttpClient http;
        http.ok = true;
        http.body = R"({"data":[{"images":{"large":"https://l/x.png"}}]})";
        PokemonCardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Pikachu", "base1", "25");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://l/x.png");
        CHECK(http.lastUrl.find("%22Pikachu%22") != std::string::npos);
        CHECK(http.lastUrl.find("set.id%3Abase1") != std::string::npos);
        CHECK(http.lastUrl.find("number%3A25") != std::string::npos);
    }
}
