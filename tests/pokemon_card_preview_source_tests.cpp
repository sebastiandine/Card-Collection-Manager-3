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
    TEST_CASE("setId plus setNo uses localId and set.id filters without name") {
        const auto url = PokemonCardPreviewSource::buildSearchUrl(
            "Charizard", "base1", "4");
        CHECK(url.find("https://api.tcgdex.net/v2/en/cards?") == 0);
        CHECK(url.find("set.id=eq:base1") != std::string::npos);
        CHECK(url.find("localId=eq:4") != std::string::npos);
        CHECK(url.find("name=") == std::string::npos);
    }

    TEST_CASE("legacy swsh12tg is canonicalized to swsh12.5tg") {
        const auto url = PokemonCardPreviewSource::buildSearchUrl(
            "Pikachu", "swsh12tg", "TG14");
        CHECK(url.find("set.id=eq:swsh12.5tg") != std::string::npos);
        CHECK(url.find("localId=eq:TG14") != std::string::npos);
    }

    TEST_CASE("setNo with a slash is normalized to the printed number") {
        const auto url = PokemonCardPreviewSource::buildSearchUrl(
            "Charizard", "base1", "4/102");
        CHECK(url.find("localId=eq:4") != std::string::npos);
        CHECK(url.find("102") == std::string::npos);
    }

    TEST_CASE("name-only search percent-encodes the name") {
        const auto url = PokemonCardPreviewSource::buildSearchUrl(
            "Mr. Mime", "base1", "");
        CHECK(url.find("name=eq:Mr.%20Mime") != std::string::npos);
        CHECK(url.find("set.id=eq:base1") != std::string::npos);
    }
}

TEST_SUITE("PokemonCardPreviewSource::buildCardByIdUrl") {
    TEST_CASE("joins setId and normalized number with a hyphen") {
        const auto url = PokemonCardPreviewSource::buildCardByIdUrl("base1", "4");
        CHECK(url == "https://api.tcgdex.net/v2/en/cards/base1-4");
    }

    TEST_CASE("canonicalizes legacy set ids") {
        const auto url =
            PokemonCardPreviewSource::buildCardByIdUrl("swsh12tg", "TG14");
        CHECK(url == "https://api.tcgdex.net/v2/en/cards/swsh12.5tg-TG14");
    }

    TEST_CASE("strips slash form before building the id") {
        const auto url =
            PokemonCardPreviewSource::buildCardByIdUrl("base1", "4/102");
        CHECK(url == "https://api.tcgdex.net/v2/en/cards/base1-4");
    }
}

TEST_SUITE("PokemonCardPreviewSource::parseSearchResponse") {
    TEST_CASE("appends /high.png to the first card image base") {
        const std::string json = R"([
            {"id":"base1-25","localId":"25","name":"Pikachu",
             "image":"https://assets.tcgdex.net/en/base/base1/25"}
        ])";
        const auto out = PokemonCardPreviewSource::parseSearchResponse(json);
        REQUIRE(out.isOk());
        CHECK(out.value() ==
              "https://assets.tcgdex.net/en/base/base1/25/high.png");
    }

    TEST_CASE("empty array is NotFound") {
        const auto out = PokemonCardPreviewSource::parseSearchResponse("[]");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("object shape is Transient") {
        const auto out = PokemonCardPreviewSource::parseSearchResponse(R"({"data":[]})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("cards without image are NotFound") {
        const auto out = PokemonCardPreviewSource::parseSearchResponse(
            R"([{"id":"base1-1","localId":"1","name":"X"}])");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("invalid JSON is Transient") {
        const auto out = PokemonCardPreviewSource::parseSearchResponse("{not json");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }
}

TEST_SUITE("PokemonCardPreviewSource::parseCardByIdResponse") {
    TEST_CASE("returns image base with /high.png") {
        const auto out = PokemonCardPreviewSource::parseCardByIdResponse(R"({
            "id": "base1-4",
            "image": "https://assets.tcgdex.net/en/base/base1/4"
        })");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://assets.tcgdex.net/en/base/base1/4/high.png");
    }

    TEST_CASE("null image is NotFound") {
        const auto out = PokemonCardPreviewSource::parseCardByIdResponse(
            R"({"id":"base1-4","image":null})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("array shape is Transient") {
        const auto out = PokemonCardPreviewSource::parseCardByIdResponse("[]");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("invalid JSON is Transient") {
        const auto out = PokemonCardPreviewSource::parseCardByIdResponse("{not json");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }
}

TEST_SUITE("PokemonCardPreviewSource::fetchImageUrl") {
    TEST_CASE("network error is surfaced as Transient") {
        FixedHttpClient http;
        http.ok = false;
        PokemonCardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Pikachu", "base1", "");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("with setNo prefers card-by-id endpoint") {
        FixedHttpClient http;
        http.ok = true;
        http.body = R"({"id":"base1-25","image":"https://assets.tcgdex.net/en/base/base1/25"})";
        PokemonCardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Pikachu", "base1", "25");
        REQUIRE(out.isOk());
        CHECK(out.value() ==
              "https://assets.tcgdex.net/en/base/base1/25/high.png");
        CHECK(http.lastUrl == "https://api.tcgdex.net/v2/en/cards/base1-25");
    }

    TEST_CASE("falls back to search when card-by-id HTTP fails") {
        class RoutingHttp final : public IHttpClient {
        public:
            int calls = 0;
            std::string lastUrl;
            Result<std::string> get(std::string_view url) override {
                lastUrl = std::string(url);
                ++calls;
                if (url.find("/v2/en/cards?") == std::string::npos) {
                    return Result<std::string>::err("HTTP 404 from card id");
                }
                return Result<std::string>::ok(
                    R"([{"id":"base1-4","localId":"4","name":"Charizard",
                         "image":"https://assets.tcgdex.net/en/base/base1/4"}])");
            }
        } http;

        PokemonCardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Charizard", "base1", "4");
        REQUIRE(out.isOk());
        CHECK(out.value() ==
              "https://assets.tcgdex.net/en/base/base1/4/high.png");
        CHECK(http.calls == 2);
        CHECK(http.lastUrl.find("set.id=eq:base1") != std::string::npos);
        CHECK(http.lastUrl.find("localId=eq:4") != std::string::npos);
    }

    TEST_CASE("empty setNo uses name search without card-by-id") {
        FixedHttpClient http;
        http.ok = true;
        http.body = R"([{"id":"base1-25","localId":"25","name":"Pikachu",
                         "image":"https://assets.tcgdex.net/en/base/base1/25"}])";
        PokemonCardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Pikachu", "base1", "");
        REQUIRE(out.isOk());
        CHECK(http.lastUrl.find("name=eq:Pikachu") != std::string::npos);
        CHECK(http.lastUrl.find("/v2/en/cards/base1-") == std::string::npos);
    }
}

namespace {

const char* kCharizardSwsh4Detail = R"({
    "id": "swsh4",
    "name": "Vivid Voltage",
    "cards": [
        {
            "id": "swsh4-25",
            "localId": "25",
            "name": "Charizard",
            "rarity": "Rare",
            "image": "https://assets.tcgdex.net/en/swsh/swsh4/25"
        }
    ]
})";

const char* kMultiVariantDetail = R"({
    "id": "base1",
    "cards": [
        {"localId":"25","name":"Pikachu","rarity":"Common"},
        {"localId":"58","name":"Pikachu","rarity":"Rare"},
        {"localId":"1","name":"Alakazam","rarity":"Rare"}
    ]
})";

}  // namespace

TEST_SUITE("PokemonCardPreviewSource::parsePrintVariants") {
    TEST_CASE("maps localId into setNo from set detail") {
        const auto out = PokemonCardPreviewSource::parsePrintVariants(
            kCharizardSwsh4Detail, "swsh4", "Charizard");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value().front().setNo == "25");
        CHECK(out.value().front().rarity == "Rare");
    }

    TEST_CASE("filters by card name within the set") {
        const auto out = PokemonCardPreviewSource::parsePrintVariants(
            kMultiVariantDetail, "base1", "Pikachu");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(out.value()[0].setNo == "25");
        CHECK(out.value()[1].setNo == "58");
    }

    TEST_CASE("wrong card name yields error") {
        const auto out = PokemonCardPreviewSource::parsePrintVariants(
            kCharizardSwsh4Detail, "swsh4", "Blastoise");
        REQUIRE(out.isErr());
        CHECK(out.error() == "Could not auto-detect set print metadata.");
    }

    TEST_CASE("missing cards array is an error") {
        const auto out = PokemonCardPreviewSource::parsePrintVariants(
            R"({"id":"base1"})", "base1", "Pikachu");
        REQUIRE(out.isErr());
    }

    TEST_CASE("empty wanted name collects all prints in the set") {
        const auto out = PokemonCardPreviewSource::parsePrintVariants(
            kMultiVariantDetail, "base1", "");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 3u);
    }
}

TEST_SUITE("PokemonCardPreviewSource::detectPrintVariants") {
    TEST_CASE("supports auto-detect and returns first print from set detail") {
        FixedHttpClient http;
        http.body = kCharizardSwsh4Detail;
        PokemonCardPreviewSource src{http};
        CHECK(src.supportsAutoDetectPrint());
        const auto first = src.detectFirstPrint("Charizard", "swsh4");
        REQUIRE(first.isOk());
        CHECK(first.value().setNo == "25");
        CHECK(http.lastUrl == "https://api.tcgdex.net/v2/en/sets/swsh4");
    }

    TEST_CASE("falls back to cards search when set detail fails") {
        class FallbackHttpClient final : public IHttpClient {
        public:
            int calls = 0;
            Result<std::string> get(std::string_view url) override {
                ++calls;
                if (std::string(url).find("/sets/") != std::string::npos) {
                    return Result<std::string>::err("offline");
                }
                return Result<std::string>::ok(R"([
                    {"id":"base1-25","localId":"25","name":"Pikachu","rarity":"Common"},
                    {"id":"base1-58","localId":"58","name":"Pikachu","rarity":"Rare"}
                ])");
            }
        } http;

        PokemonCardPreviewSource src{http};
        const auto out = src.detectPrintVariants("Pikachu", "base1");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(http.calls == 2);
    }

    TEST_CASE("surfaces search HTTP error when set detail and search fail") {
        class AlwaysFailHttp final : public IHttpClient {
        public:
            int calls = 0;
            Result<std::string> get(std::string_view) override {
                ++calls;
                return Result<std::string>::err("offline");
            }
        } http;

        PokemonCardPreviewSource src{http};
        const auto out = src.detectPrintVariants("Pikachu", "base1");
        REQUIRE(out.isErr());
        CHECK(out.error() == "offline");
        CHECK(http.calls == 2);
    }
}

TEST_SUITE("PokemonCardPreviewSource::detectVariantsBySetNo") {
    TEST_CASE("card-by-id fills name from TCGdex response") {
        FixedHttpClient http;
        http.body = R"({
          "id":"base1-4",
          "localId":"4",
          "name":"Charmander",
          "rarity":"Common",
          "image":"https://assets.tcgdex.net/en/base/base1/4"
        })";
        PokemonCardPreviewSource src{http};
        const auto out = src.detectVariantsBySetNo("base1", "4");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].name == "Charmander");
        CHECK(out.value()[0].setNo == "4");
        CHECK(out.value()[0].rarity == "Common");
        CHECK(http.lastUrl.find("/v2/en/cards/base1-4") != std::string::npos);
    }

    TEST_CASE("search fallback rejects localIds that only share a digit prefix") {
        struct ScriptedHttp : IHttpClient {
            int n = 0;
            Result<std::string> get(std::string_view) override {
                ++n;
                if (n == 1) {
                    return Result<std::string>::err("not found");
                }
                // Fuzzy search returns both "14" and "4"; only "4" may be kept.
                return Result<std::string>::ok(R"([
                  {"id":"base1-14","localId":"14","name":"Wrong","rarity":"Common"},
                  {"id":"base1-4","localId":"4","name":"Charmander","rarity":"Common"}
                ])");
            }
        } http;
        PokemonCardPreviewSource src{http};
        const auto out = src.detectVariantsBySetNo("base1", "4");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].name == "Charmander");
        CHECK(out.value()[0].setNo == "4");
    }

    TEST_CASE("card-by-id response with mismatched localId falls through to search") {
        struct ScriptedHttp : IHttpClient {
            int n = 0;
            Result<std::string> get(std::string_view) override {
                ++n;
                if (n == 1) {
                    return Result<std::string>::ok(R"({
                      "id":"base1-14","localId":"14","name":"Wrong","rarity":"Rare"
                    })");
                }
                return Result<std::string>::ok(R"([
                  {"id":"base1-4","localId":"4","name":"Charmander","rarity":"Common"}
                ])");
            }
        } http;
        PokemonCardPreviewSource src{http};
        const auto out = src.detectVariantsBySetNo("base1", "4");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].name == "Charmander");
        CHECK(out.value()[0].setNo == "4");
    }

    TEST_CASE("empty set id is rejected") {
        FixedHttpClient http;
        PokemonCardPreviewSource src{http};
        const auto out = src.detectVariantsBySetNo("", "4");
        REQUIRE(out.isErr());
        CHECK(out.error().find("set") != std::string::npos);
    }
}
