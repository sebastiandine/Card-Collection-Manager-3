#include <doctest/doctest.h>

#include "ccm/games/pokemonjp/JapanesePokemonSetSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>

using namespace ccm;

namespace {

class RoutingHttpClient final : public IHttpClient {
public:
    std::unordered_map<std::string, std::string> bodies;
    std::string lastUrl;
    bool ok = true;
    Result<std::string> get(std::string_view url) override {
        lastUrl = std::string(url);
        if (!ok) return Result<std::string>::err("offline");
        const auto it = bodies.find(lastUrl);
        if (it == bodies.end()) return Result<std::string>::err("unknown url");
        return Result<std::string>::ok(it->second);
    }
};

}  // namespace

TEST_SUITE("JapanesePokemonSetSource helpers") {
    TEST_CASE("excludes CS* set ids") {
        CHECK(JapanesePokemonSetSource::shouldExcludeSetId("CS1a"));
        CHECK(JapanesePokemonSetSource::shouldExcludeSetId("CS4a"));
        CHECK_FALSE(JapanesePokemonSetSource::shouldExcludeSetId("PMCG1"));
        CHECK_FALSE(JapanesePokemonSetSource::shouldExcludeSetId("SV1a"));
    }

    TEST_CASE("applies SV4a name override") {
        CHECK(JapanesePokemonSetSource::applySetNameOverride("SV4a", "wrong") ==
              "シャイニートレジャーex");
        CHECK(JapanesePokemonSetSource::applySetNameOverride("PMCG1", "拡張パック") ==
              "拡張パック");
    }

    TEST_CASE("rewrites release date separators") {
        CHECK(JapanesePokemonSetSource::rewriteReleaseDate("1996-10-20") == "1996/10/20");
    }

    TEST_CASE("buildSetDetailUrl percent-encodes id") {
        CHECK(JapanesePokemonSetSource::buildSetDetailUrl("SV1a") ==
              "https://api.tcgdex.net/v2/ja/sets/SV1a");
    }
}

TEST_SUITE("JapanesePokemonSetSource::parseListResponse") {
    TEST_CASE("maps id/name and drops CS* entries") {
        const std::string json = R"([
            {"id":"PMCG1","name":"拡張パック","cardCount":{"total":102,"official":102}},
            {"id":"CS1a","name":"トリプレットビート","cardCount":{"total":1,"official":1}},
            {"id":"SV4a","name":"レイジングサーフ","cardCount":{"total":320,"official":190}}
        ])";
        const auto out = JapanesePokemonSetSource::parseListResponse(json);
        REQUIRE(out.isOk());
        // 2 from TCGdex + 11 curated products omitted by TCGdex.
        REQUIRE(out.value().size() == 13);
        CHECK(out.value()[0].id == "PMCG1");
        CHECK(out.value()[0].name == "拡張パック");
        CHECK(out.value()[1].id == "SV4a");
        CHECK(out.value()[1].name == "シャイニートレジャーex");
    }

    TEST_CASE("injects classic City Gym and Expansion Sheet products") {
        const auto out = JapanesePokemonSetSource::parseListResponse("[]");
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 11);
        const auto hasId = [&](const char* id) {
            return std::any_of(out.value().begin(), out.value().end(),
                               [&](const Set& s) { return s.id == id; });
        };
        CHECK(hasId("UnnumberedPromo"));
        CHECK(hasId("TamamushiCG"));
        CHECK(hasId("NiviCG"));
        CHECK(hasId("HanadaCG"));
        CHECK(hasId("KuchibaCG"));
        CHECK(hasId("YamabukiCG"));
        CHECK(hasId("GurenTG"));
        CHECK(hasId("ExpSheet1"));
        CHECK(hasId("ExpSheet2"));
        CHECK(hasId("ExpSheet3"));
        CHECK(hasId("SouthernIslands"));
        const Set* unnumbered = nullptr;
        const Set* tama = nullptr;
        for (const auto& s : out.value()) {
            if (s.id == "UnnumberedPromo") unnumbered = &s;
            if (s.id == "TamamushiCG") tama = &s;
        }
        REQUIRE(unnumbered != nullptr);
        CHECK(unnumbered->name == "Unnumbered Promotional cards");
        CHECK(unnumbered->releaseDate == "1996/10/15");
        REQUIRE(tama != nullptr);
        CHECK(tama->name == "Tamamushi City Gym");
        CHECK(tama->releaseDate == "1998/07/25");
    }

    TEST_CASE("does not duplicate classic products already in the list") {
        const std::string json = R"([
            {"id":"TamamushiCG","name":"already-present"}
        ])";
        const auto out = JapanesePokemonSetSource::parseListResponse(json);
        REQUIRE(out.isOk());
        int tamaCount = 0;
        for (const auto& s : out.value()) {
            if (s.id == "TamamushiCG") ++tamaCount;
        }
        CHECK(tamaCount == 1);
        CHECK(out.value().front().name == "already-present");
    }

    TEST_CASE("empty array still injects classic products") {
        const auto out = JapanesePokemonSetSource::parseListResponse("[]");
        REQUIRE(out.isOk());
        CHECK_FALSE(out.value().empty());
    }

    TEST_CASE("non-array is an error") {
        CHECK(JapanesePokemonSetSource::parseListResponse(R"({"data":[]})").isErr());
    }

    TEST_CASE("invalid JSON is an error") {
        CHECK(JapanesePokemonSetSource::parseListResponse("{not json").isErr());
    }
}

TEST_SUITE("JapanesePokemonSetSource::parseReleaseDate") {
    TEST_CASE("extracts and rewrites releaseDate") {
        const auto out = JapanesePokemonSetSource::parseReleaseDate(
            R"({"id":"PMCG1","releaseDate":"1996-10-20"})");
        REQUIRE(out.isOk());
        CHECK(out.value() == "1996/10/20");
    }

    TEST_CASE("missing releaseDate yields empty string") {
        const auto out = JapanesePokemonSetSource::parseReleaseDate(R"({"id":"X"})");
        REQUIRE(out.isOk());
        CHECK(out.value().empty());
    }
}

TEST_SUITE("JapanesePokemonSetSource::fetchAll") {
    TEST_CASE("enriches from catalog and sorts by release date") {
        RoutingHttpClient http;
        http.bodies[JapanesePokemonSetSource::kListEndpoint] = R"([
            {"id":"SV1a","name":"トリプレットビート"},
            {"id":"PMCG1","name":"拡張パック"},
            {"id":"CS1a","name":"junk"}
        ])";
        // Catalog supplies dates so detail GETs are skipped.
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {
                "PMCG1": {"name_en":"Expansion Pack","name_ja":"拡張パック","releaseDate":"1996/10/20"},
                "SV1a": {"name_en":"Triplet Beat","name_ja":"トリプレットビート","releaseDate":"2023/03/10"}
            },
            "prints": []
        })");
        REQUIRE(catalog.isOk());
        JapanesePokemonSetSource src{http, catalog.value()};
        const auto out = src.fetchAll();
        REQUIRE(out.isOk());
        // CS* dropped; 2 TCGdex + 11 curated injections.
        REQUIRE(out.value().size() == 13);
        // UnnumberedPromo (1996/10/15) sorts before Expansion Pack (1996/10/20).
        CHECK(out.value()[0].id == "UnnumberedPromo");
        CHECK(out.value()[0].name == "Unnumbered Promotional cards");
        CHECK(out.value()[0].releaseDate == "1996/10/15");
        const Set* pmcg1 = nullptr;
        bool foundSv = false;
        bool foundTama = false;
        for (const auto& s : out.value()) {
            if (s.id == "PMCG1") {
                pmcg1 = &s;
                CHECK(s.name == "Expansion Pack");
                CHECK(s.releaseDate == "1996/10/20");
            }
            if (s.id == "SV1a") {
                foundSv = true;
                CHECK(s.name == "Triplet Beat");
            }
            if (s.id == "TamamushiCG") {
                foundTama = true;
                CHECK(s.name == "Tamamushi City Gym");
            }
        }
        REQUIRE(pmcg1 != nullptr);
        CHECK(foundSv);
        CHECK(foundTama);
    }

    TEST_CASE("fetches set detail when catalog lacks release date") {
        RoutingHttpClient http;
        http.bodies[JapanesePokemonSetSource::kListEndpoint] =
            R"([{"id":"PMCG1","name":"拡張パック"}])";
        http.bodies[JapanesePokemonSetSource::buildSetDetailUrl("PMCG1")] =
            R"({"id":"PMCG1","releaseDate":"1996-10-20","cards":[]})";
        JapanesePokemonEnCatalog empty;
        JapanesePokemonSetSource src{http, empty};
        const auto out = src.fetchAll();
        REQUIRE(out.isOk());
        REQUIRE(out.value().size() == 12);  // PMCG1 + 11 curated
        const Set* pmcg1 = nullptr;
        for (const auto& s : out.value()) {
            if (s.id == "PMCG1") {
                pmcg1 = &s;
                break;
            }
        }
        REQUIRE(pmcg1 != nullptr);
        CHECK(pmcg1->releaseDate == "1996/10/20");
        // Without catalog EN, never keep Japanese TCGdex names in Set.name.
        CHECK(pmcg1->name == "PMCG1");
    }

    TEST_CASE("CJK set names fall back to set id even without catalog") {
        RoutingHttpClient http;
        http.bodies[JapanesePokemonSetSource::kListEndpoint] =
            R"([{"id":"PMCG3","name":"化石の秘密"}])";
        http.bodies[JapanesePokemonSetSource::buildSetDetailUrl("PMCG3")] =
            R"({"id":"PMCG3","releaseDate":"1997-06-21"})";
        JapanesePokemonEnCatalog empty;
        JapanesePokemonSetSource src{http, empty};
        const auto out = src.fetchAll();
        REQUIRE(out.isOk());
        const Set* pmcg3 = nullptr;
        for (const auto& s : out.value()) {
            if (s.id == "PMCG3") {
                pmcg3 = &s;
                break;
            }
        }
        REQUIRE(pmcg3 != nullptr);
        CHECK(pmcg3->name == "PMCG3");
    }

    TEST_CASE("network error on list is surfaced") {
        RoutingHttpClient http;
        http.ok = false;
        JapanesePokemonEnCatalog empty;
        JapanesePokemonSetSource src{http, empty};
        CHECK(src.fetchAll().isErr());
    }

    TEST_CASE("augmentCachedSets injects classic products into a cached list") {
        RoutingHttpClient http;
        JapanesePokemonEnCatalog empty;
        JapanesePokemonSetSource src{http, empty};
        std::vector<Set> cached;
        Set pmcg2;
        pmcg2.id = "PMCG2";
        pmcg2.name = "Pokémon Jungle";
        pmcg2.releaseDate = "1997/03/05";
        cached.push_back(std::move(pmcg2));
        src.augmentCachedSets(cached);
        REQUIRE(cached.size() == 12);
        bool foundTama = false;
        bool foundUnnumbered = false;
        for (const auto& s : cached) {
            if (s.id == "TamamushiCG") {
                foundTama = true;
                CHECK(s.name == "Tamamushi City Gym");
            }
            if (s.id == "UnnumberedPromo") {
                foundUnnumbered = true;
                CHECK(s.name == "Unnumbered Promotional cards");
            }
        }
        CHECK(foundTama);
        CHECK(foundUnnumbered);
    }

    TEST_CASE("augmentCachedSets restores English names from catalog") {
        RoutingHttpClient http;
        const auto catalog = JapanesePokemonEnCatalog::parse(R"({
            "sets": {
                "PMCG2": {"name_en":"Pokémon Jungle","name_ja":"ポケモンジャングル","releaseDate":"1997/03/05"}
            },
            "prints": []
        })");
        REQUIRE(catalog.isOk());
        JapanesePokemonSetSource src{http, catalog.value()};
        std::vector<Set> cached;
        Set pmcg2;
        pmcg2.id = "PMCG2";
        pmcg2.name = "PMCG2";  // stale cache stored the id as the display name
        pmcg2.releaseDate = "1997/03/05";
        cached.push_back(std::move(pmcg2));
        src.augmentCachedSets(cached);
        const Set* jungle = nullptr;
        for (const auto& s : cached) {
            if (s.id == "PMCG2") {
                jungle = &s;
                break;
            }
        }
        REQUIRE(jungle != nullptr);
        CHECK(jungle->name == "Pokémon Jungle");
    }
}
