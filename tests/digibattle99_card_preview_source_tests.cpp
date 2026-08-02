#include <doctest/doctest.h>

#include "ccm/games/digibattle99/DigiBattle99CardPreviewSource.hpp"
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

TEST_SUITE("DigiBattle99CardPreviewSource::normalizeCardNumber") {
    TEST_CASE("uppercases alphabetic prefix without zero-padding") {
        CHECK(DigiBattle99CardPreviewSource::normalizeCardNumber("bo-88") == "BO-88");
        CHECK(DigiBattle99CardPreviewSource::normalizeCardNumber("st-01") == "ST-01");
        CHECK(DigiBattle99CardPreviewSource::normalizeCardNumber("  MO-06 ") == "MO-06");
    }
}

TEST_SUITE("DigiBattle99CardPreviewSource::buildImageUrl") {
    TEST_CASE("builds CDN jpeg URL from card id") {
        CHECK(DigiBattle99CardPreviewSource::buildImageUrl("ST-01") ==
              "https://images.digimoncard.io/images/cards/ST-01.jpg");
        CHECK(DigiBattle99CardPreviewSource::buildImageUrl("bo-115") ==
              "https://images.digimoncard.io/images/cards/BO-115.jpg");
    }
}

TEST_SUITE("DigiBattle99CardPreviewSource::buildSearchUrl") {
    TEST_CASE("percent-encodes name pack and series") {
        const auto url = DigiBattle99CardPreviewSource::buildSearchUrl(
            "Agumon", "Series 1 Starter Set", "");
        CHECK(url.find("https://digimoncard.io/api-public/search.php?series=") == 0);
        CHECK(url.find("Digimon%20Digi-Battle%20Card%20Game") != std::string::npos);
        CHECK(url.find("&n=Agumon") != std::string::npos);
        CHECK(url.find("&pack=Series%201%20Starter%20Set") != std::string::npos);
        CHECK(url.find("&card=") == std::string::npos);
    }

    TEST_CASE("includes card= when setNo is present") {
        const auto url = DigiBattle99CardPreviewSource::buildSearchUrl(
            "Agumon", "Series 1 Starter Set", "st-01");
        CHECK(url.find("&card=ST-01") != std::string::npos);
    }

    TEST_CASE("empty pack omits pack clause") {
        const auto url =
            DigiBattle99CardPreviewSource::buildSearchUrl("Agumon", "", "ST-01");
        CHECK(url.find("&pack=") == std::string::npos);
        CHECK(url.find("&card=ST-01") != std::string::npos);
    }
}

TEST_SUITE("DigiBattle99CardPreviewSource::parseImageUrlFromSearch") {
    TEST_CASE("returns CDN URL for first exact name match") {
        const std::string json = R"([
            {"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]},
            {"name":"Agumon","id":"BO-115","set_name":["Series 1 Booster Pack"]}
        ])";
        const auto out =
            DigiBattle99CardPreviewSource::parseImageUrlFromSearch(json, "Agumon");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://images.digimoncard.io/images/cards/ST-01.jpg");
    }

    TEST_CASE("empty array is NotFound") {
        const auto out =
            DigiBattle99CardPreviewSource::parseImageUrlFromSearch("[]", "Agumon");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("API error object is NotFound") {
        const auto out = DigiBattle99CardPreviewSource::parseImageUrlFromSearch(
            R"({"error":"No cards found for this search."})", "Agumon");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::NotFound);
    }

    TEST_CASE("non-array is Transient") {
        const auto out =
            DigiBattle99CardPreviewSource::parseImageUrlFromSearch(R"({"meta":{}})", "Agumon");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }

    TEST_CASE("malformed JSON is Transient") {
        const auto out =
            DigiBattle99CardPreviewSource::parseImageUrlFromSearch("{not json", "Agumon");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }
}

TEST_SUITE("DigiBattle99CardPreviewSource::fetchImageUrl") {
    TEST_CASE("setNo present skips HTTP and returns CDN URL") {
        FixedHttpClient http;
        DigiBattle99CardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Agumon", "Series 1 Starter Set", "st-01");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://images.digimoncard.io/images/cards/ST-01.jpg");
        CHECK(http.lastUrl.empty());
    }

    TEST_CASE("empty setNo searches and parses") {
        FixedHttpClient http;
        http.body = R"([{"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]}])";
        DigiBattle99CardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Agumon", "Series 1 Starter Set", "");
        REQUIRE(out.isOk());
        CHECK(out.value() == "https://images.digimoncard.io/images/cards/ST-01.jpg");
        CHECK(http.lastUrl.find("search.php") != std::string::npos);
        CHECK(http.lastUrl.find("n=Agumon") != std::string::npos);
    }

    TEST_CASE("HTTP failure is Transient") {
        FixedHttpClient http;
        http.ok = false;
        DigiBattle99CardPreviewSource src{http};
        const auto out = src.fetchImageUrl("Agumon", "Series 1 Starter Set", "");
        REQUIRE(out.isErr());
        CHECK(out.error().kind == PreviewLookupError::Kind::Transient);
    }
}

TEST_SUITE("DigiBattle99CardPreviewSource::parsePrintVariants") {
    TEST_CASE("collects distinct card ids for name+pack") {
        const std::string json = R"([
            {"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]},
            {"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]},
            {"name":"Agumon","id":"BO-115","set_name":["Series 1 Booster Pack"]},
            {"name":"Greymon","id":"ST-02","set_name":["Series 1 Starter Set"]}
        ])";
        const auto out = DigiBattle99CardPreviewSource::parsePrintVariants(
            json, "Series 1 Starter Set", "Agumon");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].setNo == "ST-01");
    }

    TEST_CASE("pack miss with exact name returns error") {
        const std::string json = R"([
            {"name":"Agumon","id":"BO-115","set_name":["Series 1 Booster Pack"]}
        ])";
        const auto out = DigiBattle99CardPreviewSource::parsePrintVariants(
            json, "Series 1 Starter Set", "Agumon");
        CHECK(out.isErr());
    }
}

TEST_SUITE("DigiBattle99CardPreviewSource::detectPrintVariants") {
    TEST_CASE("round-trips through FixedHttpClient") {
        FixedHttpClient http;
        http.body = R"([
            {"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]},
            {"name":"Agumon","id":"ST-126","set_name":["Series 1 Starter Set"]}
        ])";
        DigiBattle99CardPreviewSource src{http};
        const auto out = src.detectPrintVariants("Agumon", "Series 1 Starter Set");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 2);
        CHECK(out.value()[0].setNo == "ST-01");
        CHECK(out.value()[1].setNo == "ST-126");
    }
}

TEST_SUITE("DigiBattle99CardPreviewSource::detectVariantsBySetNo") {
    TEST_CASE("card id search fills name within pack") {
        FixedHttpClient http;
        http.body = R"([
            {"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]}
        ])";
        DigiBattle99CardPreviewSource src{http};
        const auto out = src.detectVariantsBySetNo("Series 1 Starter Set", "st-01");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].name == "Agumon");
        CHECK(out.value()[0].setNo == "ST-01");
        CHECK(http.lastUrl.find("card=ST-01") != std::string::npos);
    }

    TEST_CASE("digits-only 1 matches ST-01 not ST-11 from fuzzy API hits") {
        FixedHttpClient http;
        http.body = R"([
            {"name":"Patamon","id":"ST-11","set_name":["Series 1 Starter Set"]},
            {"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]},
            {"name":"Other","id":"BO-1","set_name":["Booster 1"]}
        ])";
        DigiBattle99CardPreviewSource src{http};
        const auto out = src.detectVariantsBySetNo("Series 1 Starter Set", "1");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].name == "Agumon");
        CHECK(out.value()[0].setNo == "ST-01");
    }

    TEST_CASE("digits-only 11 matches ST-11 not ST-01") {
        FixedHttpClient http;
        http.body = R"([
            {"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]},
            {"name":"Patamon","id":"ST-11","set_name":["Series 1 Starter Set"]}
        ])";
        DigiBattle99CardPreviewSource src{http};
        const auto out = src.detectVariantsBySetNo("Series 1 Starter Set", "11");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 1);
        CHECK(out.value()[0].name == "Patamon");
        CHECK(out.value()[0].setNo == "ST-11");
    }

    TEST_CASE("empty pack is rejected") {
        FixedHttpClient http;
        DigiBattle99CardPreviewSource src{http};
        const auto out = src.detectVariantsBySetNo("", "ST-01");
        REQUIRE(out.isErr());
        CHECK(out.error().find("set") != std::string::npos);
    }
}
