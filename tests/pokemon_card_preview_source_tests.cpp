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

    TEST_CASE("empty setId omits the set.id clause") {
        const auto url =
            PokemonCardPreviewSource::buildSearchUrl("Pikachu", "", "25");
        CHECK(url.find("set.id") == std::string::npos);
        CHECK(url.find("number%3A25") != std::string::npos);
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

    TEST_CASE("empty data array is classified as NotFound (negative-cacheable)") {
        const auto out = PokemonCardPreviewSource::parseResponse(R"({"data":[]})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("missing data array is classified as Transient (schema deviation)") {
        const auto out = PokemonCardPreviewSource::parseResponse(R"({"meta":{}})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("'data' present but not an array is Transient") {
        const auto out = PokemonCardPreviewSource::parseResponse(R"({"data":{}})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("'images' present but not an object is NotFound") {
        const auto out =
            PokemonCardPreviewSource::parseResponse(R"({"data":[{"images":[]}]})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("large unusable type falls back to small string") {
        const auto out = PokemonCardPreviewSource::parseResponse(R"({
            "data":[{"images":{"large":123,"small":"https://only.small/img.png"}}]
        })");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://only.small/img.png");
    }

    TEST_CASE("no usable large or small string yields NotFound") {
        const auto out = PokemonCardPreviewSource::parseResponse(R"({
            "data":[{"images":{"large":null,"small":false}}]
        })");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("entry without images is classified as NotFound") {
        const auto out = PokemonCardPreviewSource::parseResponse(
            R"({"data":[{"name":"Pikachu"}]})");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("invalid JSON is classified as Transient") {
        const auto out = PokemonCardPreviewSource::parseResponse("{not json");
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

namespace {

const char* kCharizardSwsh4 = R"({
    "data": [
        {
            "name": "Charizard",
            "number": "25",
            "rarity": "Rare",
            "set": {
                "id": "swsh4",
                "name": "Vivid Voltage",
                "printedTotal": 185
            }
        }
    ]
})";

const char* kMultiVariantPayload = R"({
    "data": [
        {
            "name": "Pikachu",
            "number": "25",
            "rarity": "Common",
            "set": {"id": "base1", "printedTotal": 102}
        },
        {
            "name": "Pikachu",
            "number": "58",
            "rarity": "Rare",
            "set": {"id": "base1", "printedTotal": 102}
        },
        {
            "name": "Pikachu",
            "number": "25",
            "rarity": "Common",
            "set": {"id": "base2", "printedTotal": 64}
        }
    ]
})";

}  // namespace

TEST_SUITE("PokemonCardPreviewSource::parsePrintVariants") {
    TEST_CASE("maps API number into setNo without printedTotal suffix") {
        const auto out =
            PokemonCardPreviewSource::parsePrintVariants(kCharizardSwsh4, "swsh4", "Charizard");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value().front().setNo == "25");
        CHECK(out.value().front().rarity == "Rare");
    }

    TEST_CASE("filters by set id and keeps multiple numbers in the same set") {
        const auto out =
            PokemonCardPreviewSource::parsePrintVariants(kMultiVariantPayload, "base1", "Pikachu");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(out.value()[0].setNo == "25");
        CHECK(out.value()[1].setNo == "58");
    }

    TEST_CASE("wrong set id yields explicit error when name and set are supplied") {
        const auto out =
            PokemonCardPreviewSource::parsePrintVariants(kCharizardSwsh4, "base1", "Charizard");
        REQUIRE(out.isErr());
        CHECK(out.error() == "Could not auto-detect set print metadata.");
    }

    TEST_CASE("wrong card name is filtered out") {
        const auto out =
            PokemonCardPreviewSource::parsePrintVariants(kCharizardSwsh4, "swsh4", "Blastoise");
        REQUIRE(out.isErr());
        CHECK(out.error() == "Could not auto-detect set print metadata.");
    }

    TEST_CASE("empty data array yields error") {
        const auto out =
            PokemonCardPreviewSource::parsePrintVariants(R"({"data":[]})", "base1", "Pikachu");
        REQUIRE(out.isErr());
        CHECK(out.error() == "Pokemon TCG returned no matching cards.");
    }

    TEST_CASE("name-only payload still filters to requested set id") {
        const auto out =
            PokemonCardPreviewSource::parsePrintVariants(kMultiVariantPayload, "base2", "Pikachu");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value().front().setNo == "25");
    }

    TEST_CASE("keeps bare number when printedTotal is zero") {
        const auto out = PokemonCardPreviewSource::parsePrintVariants(R"({
            "data": [
                {
                    "name": "Promo",
                    "number": "7",
                    "rarity": "Promo",
                    "set": {"id": "promo1", "printedTotal": 0}
                }
            ]
        })",
            "promo1", "Promo");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value().front().setNo == "7");
    }
}

TEST_SUITE("PokemonCardPreviewSource::detectPrintVariants") {
    TEST_CASE("supports auto-detect and returns first print") {
        FixedHttpClient http;
        http.body = kCharizardSwsh4;
        PokemonCardPreviewSource src{http};
        CHECK(src.supportsAutoDetectPrint());
        const auto first = src.detectFirstPrint("Charizard", "swsh4");
        REQUIRE(first.isOk());
        CHECK(first.value().setNo == "25");
    }

    TEST_CASE("uses slim set-scoped search URL without number clause") {
        FixedHttpClient http;
        http.body = kCharizardSwsh4;
        PokemonCardPreviewSource src{http};
        const auto out = src.detectPrintVariants("Charizard", "swsh4");
        REQUIRE(out.isOk());
        CHECK(http.lastUrl.find("number%3A") == std::string::npos);
        CHECK(http.lastUrl.find("set.id%3Aswsh4") != std::string::npos);
        CHECK(http.lastUrl.find("select=name,number,rarity,set") != std::string::npos);
        CHECK(http.lastUrl.find("pageSize=50") != std::string::npos);
    }

    TEST_CASE("buildDetectSearchUrl requests only parser fields") {
        const auto url = PokemonCardPreviewSource::buildDetectSearchUrl("Charizard", "swsh4");
        CHECK(url.find("select=name,number,rarity,set") != std::string::npos);
        CHECK(url.find("pageSize=50") != std::string::npos);
    }

    TEST_CASE("retries name-only query when the set-scoped request fails") {
        class FallbackHttpClient final : public IHttpClient {
        public:
            int calls = 0;
            Result<std::string> get(std::string_view url) override {
                ++calls;
                if (calls == 1) return Result<std::string>::err("offline");
                if (url.find("set.id") != std::string::npos) {
                    return Result<std::string>::err("unexpected set-scoped retry");
                }
                return Result<std::string>::ok(kMultiVariantPayload);
            }
        } http;

        PokemonCardPreviewSource src{http};
        const auto out = src.detectPrintVariants("Pikachu", "base1");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(http.calls == 2);
    }

    TEST_CASE("detectFirstPrint errors when variant listing succeeds but is empty") {
        FixedHttpClient http;
        http.body = R"({"data":[{"name":"Promo","number":"","rarity":"","set":{"id":"promo1"}}]})";
        PokemonCardPreviewSource src{http};
        const auto out = src.detectFirstPrint("Promo", "promo1");
        REQUIRE(out.isErr());
        CHECK(out.error() == "Could not auto-detect set print metadata.");
    }

    TEST_CASE("parsePrintVariants ignores cards whose set field is not an object") {
        const auto out = PokemonCardPreviewSource::parsePrintVariants(R"({
            "data":[
                {"name":"Pikachu","number":"25","rarity":"Common","set":"not-an-object"},
                {"name":"Pikachu","number":"26","rarity":"Rare","set":{"id":"base1"}}
            ]
        })",
            "base1", "Pikachu");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value().front().setNo == "26");
    }
}
