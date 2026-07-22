#include <doctest/doctest.h>

#include "ccm/games/digibattle99/DigiBattle99SetSource.hpp"
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

TEST_SUITE("DigiBattle99SetSource::slugifyPackName") {
    TEST_CASE("slugifies pack display names") {
        CHECK(DigiBattle99SetSource::slugifyPackName("Series 1 Starter Set") ==
              "series-1-starter-set");
        CHECK(DigiBattle99SetSource::slugifyPackName("Digimon The Movie Promo Cards") ==
              "digimon-the-movie-promo-cards");
    }
}

TEST_SUITE("DigiBattle99SetSource::parseResponse") {
    TEST_CASE("derives unique packs with curated release dates") {
        const std::string json = R"([
            {"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]},
            {"name":"MetalGreymon","id":"BO-01","set_name":["Series 1 Booster Pack"]},
            {"name":"Agumon","id":"ST-126","set_name":["Series 1 Starter Set"]},
            {"name":"Promo","id":"MO-06","set_name":["Digimon The Movie Promo Cards"]}
        ])";

        const auto out = DigiBattle99SetSource::parseResponse(json);
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 3);
        // Same curated date for Series 1 products → secondary sort by name:
        // "Booster" before "Starter".
        CHECK(out.value()[0].id == "series-1-booster-pack");
        CHECK(out.value()[0].name == "Series 1 Booster Pack");
        CHECK(out.value()[0].releaseDate == "1999/06/01");
        CHECK(out.value()[1].id == "series-1-starter-set");
        CHECK(out.value()[1].name == "Series 1 Starter Set");
        CHECK(out.value()[1].releaseDate == "1999/06/01");
        CHECK(out.value()[2].id == "digimon-the-movie-promo-cards");
        CHECK(out.value()[2].releaseDate == "2000/10/01");
    }

    TEST_CASE("sorts by release date then name") {
        const std::string json = R"([
            {"name":"A","id":"ST-1","set_name":["Street Starter Set 2"]},
            {"name":"B","id":"ST-2","set_name":["Series 1 Starter Set"]},
            {"name":"C","id":"ST-3","set_name":["Street Starter Set 1"]}
        ])";
        const auto out = DigiBattle99SetSource::parseResponse(json);
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 3);
        CHECK(out.value()[0].name == "Series 1 Starter Set");
        CHECK(out.value()[1].name == "Street Starter Set 1");
        CHECK(out.value()[2].name == "Street Starter Set 2");
    }

    TEST_CASE("empty array returns an empty list") {
        const auto out = DigiBattle99SetSource::parseResponse("[]");
        REQUIRE(out.isOk());
        CHECK(out.value().empty());
    }

    TEST_CASE("non-array object with error is an error") {
        const auto out = DigiBattle99SetSource::parseResponse(
            R"({"error":"No cards found for this search."})");
        CHECK(out.isErr());
    }

    TEST_CASE("missing top-level array returns an error") {
        const auto out = DigiBattle99SetSource::parseResponse(R"({"meta":{}})");
        CHECK(out.isErr());
    }

    TEST_CASE("invalid JSON returns an error") {
        const auto out = DigiBattle99SetSource::parseResponse("{not json");
        CHECK(out.isErr());
    }
}

TEST_SUITE("DigiBattle99SetSource::fetchAll") {
    TEST_CASE("network error is surfaced as a Result error") {
        FixedHttpClient http;
        http.ok = false;
        DigiBattle99SetSource src{http};
        CHECK(src.fetchAll().isErr());
    }

    TEST_CASE("network success hits the Digi-Battle search endpoint") {
        FixedHttpClient http;
        http.ok = true;
        http.body = R"([{"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]}])";
        DigiBattle99SetSource src{http};
        const auto out = src.fetchAll();
        REQUIRE(out.isOk());
        CHECK(out.value().front().id == "series-1-starter-set");
        CHECK(http.lastUrl == DigiBattle99SetSource::kEndpoint);
    }

    TEST_CASE("fetchAllWithCatalog returns sets and pack cards in one GET") {
        FixedHttpClient http;
        http.ok = true;
        http.body = R"([
            {"name":"Agumon","id":"st-01","set_name":["Series 1 Starter Set","Series 1 Booster Pack"]},
            {"name":"Greymon","id":"ST-02","set_name":["Series 1 Starter Set"]}
        ])";
        DigiBattle99SetSource src{http};
        const auto out = src.fetchAllWithCatalog();
        REQUIRE(out.isOk());
        CHECK(out.value().sets.size() == 2);
        const auto* starter = out.value().catalog.findPack("series-1-starter-set");
        REQUIRE(starter != nullptr);
        REQUIRE(starter->cards.size() == 2);
        CHECK(starter->cards[0].setNo == "ST-01");
        CHECK(starter->cards[0].name == "Agumon");
        const auto* booster = out.value().catalog.findPack("series-1-booster-pack");
        REQUIRE(booster != nullptr);
        REQUIRE(booster->cards.size() == 1);
        CHECK(booster->cards[0].setNo == "ST-01");
        CHECK(http.lastUrl == DigiBattle99SetSource::kEndpoint);
    }
}

TEST_SUITE("DigiBattle99SetSource::parseCatalog") {
    TEST_CASE("lists a card under every pack in set_name") {
        const std::string json = R"([
            {"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set","Series 1 Booster Pack"]},
            {"name":"MetalGreymon","id":"BO-01","set_name":["Series 1 Booster Pack"]}
        ])";
        const auto out = DigiBattle99SetSource::parseCatalog(json);
        REQUIRE(out.isOk());
        REQUIRE(out.value().packs.size() == 2);

        const auto* booster = out.value().findPack("series-1-booster-pack");
        REQUIRE(booster != nullptr);
        REQUIRE(booster->cards.size() == 2);
        CHECK(booster->cards[0].setNo == "BO-01");
        CHECK(booster->cards[1].setNo == "ST-01");

        const auto* starter = out.value().findPack("series-1-starter-set");
        REQUIRE(starter != nullptr);
        REQUIRE(starter->cards.size() == 1);
        CHECK(starter->cards[0].setNo == "ST-01");
    }

    TEST_CASE("dedupes the same setNo within one pack") {
        const std::string json = R"([
            {"name":"Agumon","id":"ST-01","set_name":["Series 1 Starter Set"]},
            {"name":"Agumon Alt","id":"ST-01","set_name":["Series 1 Starter Set"]}
        ])";
        const auto out = DigiBattle99SetSource::parseCatalog(json);
        REQUIRE(out.isOk());
        const auto* starter = out.value().findPack("series-1-starter-set");
        REQUIRE(starter != nullptr);
        REQUIRE(starter->cards.size() == 1);
        CHECK(starter->cards[0].name == "Agumon");
    }

    TEST_CASE("empty array returns an empty catalog") {
        const auto out = DigiBattle99SetSource::parseCatalog("[]");
        REQUIRE(out.isOk());
        CHECK(out.value().empty());
    }

    TEST_CASE("error object is an error") {
        const auto out = DigiBattle99SetSource::parseCatalog(
            R"({"error":"No cards found for this search."})");
        CHECK(out.isErr());
    }
}
