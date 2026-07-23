#include <doctest/doctest.h>

#include "ccm/games/yugioh/YuGiOhSetSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <vector>

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

TEST_SUITE("YuGiOhSetSource::parseResponse") {
    TEST_CASE("maps set_code/set_name/tcg_date") {
        const std::string json = R"([
            {"set_name":"Set A","set_code":"AAA","tcg_date":"2020-01-01"},
            {"set_name":"Set B","set_code":"BBB","tcg_date":"2021-02-03"}
        ])";
        const auto out = YuGiOhSetSource::parseResponse(json);
        REQUIRE(out.isOk());
        bool foundA = false;
        bool foundB = false;
        for (const auto& set : out.value()) {
            if (set.id == "AAA" && set.name == "Set A" && set.releaseDate == "2020/01/01") {
                foundA = true;
            }
            if (set.id == "BBB" && set.name == "Set B" && set.releaseDate == "2021/02/03") {
                foundB = true;
            }
        }
        CHECK(foundA);
        CHECK(foundB);
    }

    TEST_CASE("sorts by release date ascending") {
        const auto out = YuGiOhSetSource::parseResponse(R"([
            {"set_name":"New","set_code":"N","tcg_date":"2024-01-01"},
            {"set_name":"Old","set_code":"O","tcg_date":"2010-01-01"}
        ])");
        REQUIRE(out.isOk());
        CHECK(out.value().front().id == "O");
        CHECK(out.value().back().id == "N");
    }

    TEST_CASE("missing array returns error") {
        CHECK(YuGiOhSetSource::parseResponse(R"({"data":[]})").isErr());
    }

    TEST_CASE("empty upstream array still appends missing 25th aliases") {
        const auto out = YuGiOhSetSource::parseResponse("[]");
        REQUIRE(out.isOk());
        CHECK(out.value().size() == 6);
        bool foundLob25th = false;
        bool foundIoc25th = false;
        for (const auto& set : out.value()) {
            if (set.id == "LOB-25TH") foundLob25th = true;
            if (set.id == "IOC-25TH") foundIoc25th = true;
        }
        CHECK(foundLob25th);
        CHECK(foundIoc25th);
    }

    TEST_CASE("adds 25th Anniversary aliases when upstream list misses them") {
        const auto out = YuGiOhSetSource::parseResponse(R"([
            {"set_name":"Legend of Blue Eyes White Dragon","set_code":"LOB","tcg_date":"2002-03-08"}
        ])");
        REQUIRE(out.isOk());

        bool foundLob25th = false;
        bool foundIoc25th = false;
        for (const auto& set : out.value()) {
            if (set.name == "Legend of Blue Eyes White Dragon (25th Anniversary Edition)"
                && set.id == "LOB-25TH") {
                foundLob25th = true;
            }
            if (set.name == "Invasion of Chaos (25th Anniversary Edition)" && set.id == "IOC-25TH") {
                foundIoc25th = true;
            }
        }
        CHECK(foundLob25th);
        CHECK(foundIoc25th);
    }

    TEST_CASE("does not duplicate aliases that already exist by name") {
        const auto out = YuGiOhSetSource::parseResponse(R"json([
            {"set_name":"Legend of Blue Eyes White Dragon (25th Anniversary Edition)","set_code":"LOB-25TH","tcg_date":"2023-04-20"}
        ])json");
        REQUIRE(out.isOk());

        int aliasCount = 0;
        for (const auto& set : out.value()) {
            if (set.name == "Legend of Blue Eyes White Dragon (25th Anniversary Edition)") {
                ++aliasCount;
            }
        }
        CHECK(aliasCount == 1);
    }

    TEST_CASE("malformed json returns parse error") {
        const auto out = YuGiOhSetSource::parseResponse("{bad json");
        REQUIRE(out.isErr());
        CHECK(out.error().find("YGOPRODeck set parse error:") != std::string::npos);
    }

    TEST_CASE("missing fields fall back to empty strings and keep parsing") {
        const std::string json = R"([
            {"set_name":"Set A"},
            {"set_code":"BBB","tcg_date":"2021-02-03"}
        ])";
        const auto out = YuGiOhSetSource::parseResponse(json);
        REQUIRE(out.isOk());
        bool foundMissingCode = false;
        bool foundMissingName = false;
        for (const auto& set : out.value()) {
            if (set.name == "Set A" && set.id.empty() && set.releaseDate.empty()) {
                foundMissingCode = true;
            }
            if (set.id == "BBB" && set.name.empty() && set.releaseDate == "2021/02/03") {
                foundMissingName = true;
            }
        }
        CHECK(foundMissingCode);
        CHECK(foundMissingName);
    }

    TEST_CASE("preserves slash-formatted dates and normalizes hyphen dates") {
        const std::string json = R"([
            {"set_name":"Slash Date","set_code":"S","tcg_date":"2024/01/01"},
            {"set_name":"Hyphen Date","set_code":"H","tcg_date":"2024-01-02"}
        ])";
        const auto out = YuGiOhSetSource::parseResponse(json);
        REQUIRE(out.isOk());
        bool sawSlash = false;
        bool sawHyphenNormalized = false;
        for (const auto& set : out.value()) {
            if (set.id == "S" && set.releaseDate == "2024/01/01") sawSlash = true;
            if (set.id == "H" && set.releaseDate == "2024/01/02") sawHyphenNormalized = true;
        }
        CHECK(sawSlash);
        CHECK(sawHyphenNormalized);
    }
}

TEST_SUITE("YuGiOhSetSource::fetchAll") {
    TEST_CASE("network success parses and hits endpoint") {
        FixedHttpClient http;
        http.body = R"([{"set_name":"Set X","set_code":"X","tcg_date":"2020-01-01"}])";
        YuGiOhSetSource src{http};
        const auto out = src.fetchAll();
        REQUIRE(out.isOk());
        CHECK(out.value().front().id == "X");
        CHECK(http.lastUrl == "https://db.ygoprodeck.com/api/v7/cardsets.php");
    }

    TEST_CASE("network error is propagated") {
        FixedHttpClient http;
        http.ok = false;
        YuGiOhSetSource src{http};
        const auto out = src.fetchAll();
        REQUIRE(out.isErr());
        CHECK(out.error() == "offline");
    }
}

TEST_SUITE("YuGiOhSetSource::parseCatalog") {
    TEST_CASE("groups by set_name resolved to Set.id and dedupes printing slots") {
        const std::vector<Set> sets{
            Set{"LOB", "Legend of Blue Eyes White Dragon", "2002/03/08"},
            Set{"MRD", "Metal Raiders", "2002/06/26"},
        };
        const std::string json = R"({
            "data": [
                {
                    "name": "Blue-Eyes White Dragon",
                    "card_sets": [
                        {"set_name":"Legend of Blue Eyes White Dragon","set_code":"LOB-001","set_rarity":"Ultra Rare"},
                        {"set_name":"Metal Raiders","set_code":"MRD-010","set_rarity":"Ultra Rare"}
                    ]
                },
                {
                    "name": "Dark Magician",
                    "card_sets": [
                        {"set_name":"Legend of Blue Eyes White Dragon","set_code":"LOB-005","set_rarity":"Ultra Rare"},
                        {"set_name":"Legend of Blue Eyes White Dragon","set_code":"LOB-EN005","set_rarity":"Ultra Rare"},
                        {"set_name":"Legend of Blue Eyes White Dragon","set_code":"LOB-E003","set_rarity":"Ultra Rare"}
                    ]
                }
            ]
        })";
        const auto out = YuGiOhSetSource::parseCatalog(json, sets);
        REQUIRE(out.isOk());
        const auto* lob = out.value().findPack("LOB");
        REQUIRE(lob != nullptr);
        REQUIRE(lob->cards.size() == 2);
        bool sawBe = false;
        bool sawDm = false;
        for (const auto& c : lob->cards) {
            if (c.name == "Blue-Eyes White Dragon" && c.setNo == "LOB-001") sawBe = true;
            if (c.name == "Dark Magician" && c.setNo == "LOB-EN005") sawDm = true;
        }
        CHECK(sawBe);
        CHECK(sawDm);

        const auto* mrd = out.value().findPack("MRD");
        REQUIRE(mrd != nullptr);
        REQUIRE(mrd->cards.size() == 1);
        CHECK(mrd->cards[0].setNo == "MRD-010");
    }

    TEST_CASE("missing data array returns error") {
        CHECK(YuGiOhSetSource::parseCatalog(R"([])", {}).isErr());
        CHECK(YuGiOhSetSource::parseCatalog(R"({"data":{}})", {}).isErr());
    }
}

namespace {

class RoutingHttpClient final : public IHttpClient {
public:
    std::string setsBody;
    std::string infoBody;
    bool setsOk = true;
    bool infoOk = true;
    std::vector<std::string> urls;

    Result<std::string> get(std::string_view url) override {
        urls.emplace_back(url);
        if (url == YuGiOhSetSource::kEndpoint) {
            return setsOk ? Result<std::string>::ok(setsBody)
                          : Result<std::string>::err("sets offline");
        }
        if (url == YuGiOhSetSource::kCardInfoEndpoint) {
            return infoOk ? Result<std::string>::ok(infoBody)
                          : Result<std::string>::err("info offline");
        }
        return Result<std::string>::err("unexpected url");
    }
};

}  // namespace

TEST_SUITE("YuGiOhSetSource::fetchAllWithCatalog") {
    TEST_CASE("fetches sets then cardinfo and returns both") {
        RoutingHttpClient http;
        http.setsBody = R"([{"set_name":"Legend of Blue Eyes White Dragon","set_code":"LOB","tcg_date":"2002-03-08"}])";
        http.infoBody = R"({
            "data": [{
                "name": "Blue-Eyes White Dragon",
                "card_sets": [
                    {"set_name":"Legend of Blue Eyes White Dragon","set_code":"LOB-001","set_rarity":"Ultra Rare"}
                ]
            }]
        })";
        YuGiOhSetSource src{http};
        const auto out = src.fetchAllWithCatalog();
        REQUIRE(out.isOk());
        REQUIRE(http.urls.size() == 2);
        CHECK(http.urls[0] == YuGiOhSetSource::kEndpoint);
        CHECK(http.urls[1] == YuGiOhSetSource::kCardInfoEndpoint);
        CHECK(out.value().sets.front().id == "LOB");
        REQUIRE(out.value().catalog.findPack("LOB") != nullptr);
        CHECK(out.value().catalog.findPack("LOB")->cards.size() == 1);
    }

    TEST_CASE("cardinfo failure propagates after sets succeed") {
        RoutingHttpClient http;
        http.setsBody = R"([{"set_name":"Set X","set_code":"X","tcg_date":"2020-01-01"}])";
        http.infoOk = false;
        YuGiOhSetSource src{http};
        const auto out = src.fetchAllWithCatalog();
        REQUIRE(out.isErr());
        CHECK(out.error() == "info offline");
    }
}
