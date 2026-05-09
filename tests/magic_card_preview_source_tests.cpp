#include <doctest/doctest.h>

#include "ccm/games/magic/MagicCardPreviewSource.hpp"
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

TEST_SUITE("MagicCardPreviewSource::buildSearchUrl") {
    TEST_CASE("simple name and set produce a percent-encoded query") {
        const auto url = MagicCardPreviewSource::buildSearchUrl("Lightning Bolt", "lea");
        // Spaces -> %20, quotes -> %22, colons -> %3A. setId stays as-is when
        // it only contains unreserved chars.
        CHECK(url.find("https://api.scryfall.com/cards/search?q=") == 0);
        CHECK(url.find("%22Lightning%20Bolt%22") != std::string::npos);
        CHECK(url.find("set%3Alea") != std::string::npos);
    }

    TEST_CASE("ampersand in card name is replaced with 'and' before encoding") {
        const auto url = MagicCardPreviewSource::buildSearchUrl("Fire & Ice", "abc");
        CHECK(url.find("Fire%20and%20Ice") != std::string::npos);
        CHECK(url.find("%26") == std::string::npos);
    }

    TEST_CASE("unreserved characters in setId are preserved") {
        const auto url = MagicCardPreviewSource::buildSearchUrl("X", "swsh10");
        CHECK(url.find("set%3Aswsh10") != std::string::npos);
    }
}

TEST_SUITE("MagicCardPreviewSource::parseResponse") {
    TEST_CASE("happy path returns image_uris.normal") {
        const std::string json = R"({
            "data": [
                {
                    "name": "Lightning Bolt",
                    "image_uris": {
                        "small":  "https://img.scryfall.io/small.jpg",
                        "normal": "https://img.scryfall.io/normal.jpg",
                        "large":  "https://img.scryfall.io/large.jpg"
                    }
                }
            ]
        })";
        const auto out = MagicCardPreviewSource::parseResponse(json);
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://img.scryfall.io/normal.jpg");
    }

    TEST_CASE("empty data array is classified as NotFound (negative-cacheable)") {
        const auto out = MagicCardPreviewSource::parseResponse(R"({"data":[]})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("missing data array is classified as Transient (schema deviation)") {
        // No `data` array at all means the API contract failed - this is
        // not the user's record being weird, it's the upstream not
        // talking to us right now.
        const auto out = MagicCardPreviewSource::parseResponse(R"({"meta":{}})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("entry without image_uris is classified as NotFound (double-faced cards)") {
        const std::string json = R"({
            "data": [
                {"name":"DoubleFace","card_faces":[{"image_uris":{"normal":"x"}}]}
            ]
        })";
        const auto out = MagicCardPreviewSource::parseResponse(json);
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("invalid JSON is classified as Transient") {
        const auto out = MagicCardPreviewSource::parseResponse("{not json");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }
}

TEST_SUITE("MagicCardPreviewSource::fetchImageUrl") {
    TEST_CASE("network error is surfaced as Transient") {
        FixedHttpClient http;
        http.ok = false;
        MagicCardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Lightning Bolt", "lea", "");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("network success is parsed end-to-end and uses the encoded URL") {
        FixedHttpClient http;
        http.ok = true;
        http.body = R"({"data":[{"image_uris":{"normal":"https://img/normal.jpg"}}]})";
        MagicCardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Lightning Bolt", "lea", "");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://img/normal.jpg");
        CHECK(http.lastUrl.find("%22Lightning%20Bolt%22") != std::string::npos);
        CHECK(http.lastUrl.find("set%3Alea") != std::string::npos);
    }
}
