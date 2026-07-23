#include <doctest/doctest.h>

#include "ccm/domain/PokemonCard.hpp"
#include "ccm/domain/PokemonSetCatalog.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/PokemonSetCatalogService.hpp"
#include "ccm/services/PokemonSetCompletion.hpp"
#include "fakes/InMemoryFileSystem.hpp"

#include <nlohmann/json.hpp>

using namespace ccm;
using ccm::testing::InMemoryFileSystem;

namespace {

ConfigService makeConfig(InMemoryFileSystem& fs, const std::string& dataDir) {
    Configuration c;
    c.dataStorage = dataDir;
    c.defaultGame = Game::Magic;
    fs.writeText("/app/config.json", nlohmann::json(c).dump());
    ConfigService cfg{fs, "/app/config.json", dataDir};
    cfg.initialize();
    return cfg;
}

PokemonCard makeOwned(PokemonRegion region, std::string setId, std::string setNo) {
    PokemonCard c;
    c.id = 1;
    c.name = "Owned";
    c.region = region;
    c.set.id = std::move(setId);
    c.set.name = "Set";
    c.setNo = std::move(setNo);
    c.language = Language::English;
    return c;
}

PokemonSetCatalog westCatalog() {
    PokemonSetCatalog catalog;
    PokemonSetCatalogPack base;
    base.setId = "base1";
    base.setName = "Base";
    base.cards = {
        {"4", "Charizard"},
        {"58", "Growlithe"},
        {"59", "Arcanine"},
    };
    catalog.packs.push_back(std::move(base));
    return catalog;
}

PokemonSetCatalog asiaCatalog() {
    PokemonSetCatalog catalog;
    PokemonSetCatalogPack pmcg1;
    pmcg1.setId = "PMCG1";
    pmcg1.setName = "Expansion Pack";
    pmcg1.cards = {
        {"001", "Charmander"},
        {"002", "Charmeleon"},
        {"006", "Charizard"},
    };
    catalog.packs.push_back(std::move(pmcg1));
    return catalog;
}

}  // namespace

TEST_SUITE("computePokemonSetCompletion") {
    TEST_CASE("west pack with owned card appears") {
        const auto west = westCatalog();
        const auto asia = asiaCatalog();
        std::vector<PokemonCard> collection{
            makeOwned(PokemonRegion::West, "base1", "4"),
        };
        const auto rows = computePokemonSetCompletion(collection, west, asia);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].region == PokemonRegion::West);
        CHECK(rows[0].setId == "base1");
        CHECK(rows[0].ownedUnique == 1);
        CHECK(rows[0].total == 3);
        CHECK(rows[0].percent() == 33);
    }

    TEST_CASE("asia and west do not cross-count") {
        const auto west = westCatalog();
        const auto asia = asiaCatalog();
        // Same collector-looking number, different region/set.
        PokemonCard westCard = makeOwned(PokemonRegion::West, "base1", "4");
        PokemonCard asiaCard = makeOwned(PokemonRegion::Asia, "PMCG1", "006");
        asiaCard.id = 2;

        const auto rows = computePokemonSetCompletion({westCard, asiaCard}, west, asia);
        REQUIRE(rows.size() == 2);
        CHECK(rows[0].setId == "base1");
        CHECK(rows[0].ownedUnique == 1);
        CHECK(rows[1].setId == "PMCG1");
        CHECK(rows[1].ownedUnique == 1);
    }

    TEST_CASE("region filter isolates catalogs") {
        const auto west = westCatalog();
        const auto asia = asiaCatalog();
        PokemonCard westCard = makeOwned(PokemonRegion::West, "base1", "4");
        PokemonCard asiaCard = makeOwned(PokemonRegion::Asia, "PMCG1", "001");
        asiaCard.id = 2;

        const auto westOnly = computePokemonSetCompletion(
            {westCard, asiaCard}, west, asia, PokemonRegion::West);
        REQUIRE(westOnly.size() == 1);
        CHECK(westOnly[0].setId == "base1");

        const auto asiaOnly = computePokemonSetCompletion(
            {westCard, asiaCard}, west, asia, PokemonRegion::Asia);
        REQUIRE(asiaOnly.size() == 1);
        CHECK(asiaOnly[0].setId == "PMCG1");
    }

    TEST_CASE("normalizes west 4/102 to 4") {
        const auto west = westCatalog();
        PokemonSetCatalog emptyAsia;
        std::vector<PokemonCard> collection{
            makeOwned(PokemonRegion::West, "base1", "4/102"),
        };
        const auto rows = computePokemonSetCompletion(collection, west, emptyAsia);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].ownedUnique == 1);
    }

    TEST_CASE("legacy pokemontcg West set id matches TCGdex catalog pack") {
        PokemonSetCatalog west;
        PokemonSetCatalogPack pack;
        pack.setId = "sv01";
        pack.setName = "Scarlet & Violet";
        pack.cards = {{"6", "Charizard"}};
        west.packs.push_back(std::move(pack));
        PokemonSetCatalog emptyAsia;
        std::vector<PokemonCard> collection{
            makeOwned(PokemonRegion::West, "sv1", "6"),
        };
        const auto rows = computePokemonSetCompletion(collection, west, emptyAsia);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].setId == "sv01");
        CHECK(rows[0].ownedUnique == 1);

        const auto checklist = pokemonChecklistForSet(
            collection, west, emptyAsia, PokemonRegion::West, "sv01");
        REQUIRE(checklist.size() == 1);
        CHECK(checklist[0].owned);
    }

    TEST_CASE("amount does not inflate unique ownership") {
        const auto west = westCatalog();
        PokemonSetCatalog emptyAsia;
        PokemonCard a = makeOwned(PokemonRegion::West, "base1", "4");
        a.amount = 5;
        PokemonCard b = makeOwned(PokemonRegion::West, "base1", "4");
        b.id = 2;
        PokemonCard c = makeOwned(PokemonRegion::West, "base1", "58");
        c.id = 3;
        const auto rows = computePokemonSetCompletion({a, b, c}, west, emptyAsia);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].ownedUnique == 2);
    }

    TEST_CASE("language filter hides packs with no matching language") {
        const auto west = westCatalog();
        PokemonSetCatalog emptyAsia;
        PokemonCard en = makeOwned(PokemonRegion::West, "base1", "4");
        en.language = Language::English;

        CHECK(computePokemonSetCompletion({en}, west, emptyAsia, std::nullopt,
                                          Language::German)
                  .empty());
        REQUIRE(computePokemonSetCompletion({en}, west, emptyAsia, std::nullopt,
                                            Language::English)
                    .size() == 1);
    }

    TEST_CASE("empty catalog yields no rows") {
        PokemonSetCatalog empty;
        std::vector<PokemonCard> collection{
            makeOwned(PokemonRegion::West, "base1", "4"),
        };
        CHECK(computePokemonSetCompletion(collection, empty, empty).empty());
    }
}

TEST_SUITE("pokemonChecklistForSet") {
    TEST_CASE("marks owned west cards") {
        const auto west = westCatalog();
        PokemonSetCatalog emptyAsia;
        std::vector<PokemonCard> collection{
            makeOwned(PokemonRegion::West, "base1", "58"),
        };
        const auto list = pokemonChecklistForSet(collection, west, emptyAsia,
                                                 PokemonRegion::West, "base1");
        REQUIRE(list.size() == 3);
        CHECK(list[0].setNo == "4");
        CHECK(list[0].owned == false);
        CHECK(list[1].setNo == "58");
        CHECK(list[1].owned == true);
        CHECK(list[2].setNo == "59");
        CHECK(list[2].owned == false);
    }

    TEST_CASE("asia card does not mark west checklist") {
        const auto west = westCatalog();
        const auto asia = asiaCatalog();
        std::vector<PokemonCard> collection{
            makeOwned(PokemonRegion::Asia, "PMCG1", "006"),
        };
        const auto list = pokemonChecklistForSet(collection, west, asia,
                                                 PokemonRegion::West, "base1");
        REQUIRE(list.size() == 3);
        CHECK(list[0].owned == false);
        CHECK(list[1].owned == false);
        CHECK(list[2].owned == false);
    }
}

TEST_SUITE("pokemonLanguagesInCollection") {
    TEST_CASE("region filter scopes languages") {
        PokemonCard westEn = makeOwned(PokemonRegion::West, "base1", "4");
        westEn.language = Language::English;
        PokemonCard asiaJp = makeOwned(PokemonRegion::Asia, "PMCG1", "001");
        asiaJp.id = 2;
        asiaJp.language = Language::Japanese;

        const auto all = pokemonLanguagesInCollection({westEn, asiaJp});
        REQUIRE(all.size() == 2);
        CHECK(all[0] == Language::English);
        CHECK(all[1] == Language::Japanese);

        const auto westOnly =
            pokemonLanguagesInCollection({westEn, asiaJp}, PokemonRegion::West);
        REQUIRE(westOnly.size() == 1);
        CHECK(westOnly[0] == Language::English);
    }
}

TEST_SUITE("pokemonRegionsInCollection") {
    TEST_CASE("reports regions with matching catalog packs") {
        const auto west = westCatalog();
        const auto asia = asiaCatalog();
        PokemonCard westCard = makeOwned(PokemonRegion::West, "base1", "4");
        PokemonCard asiaCard = makeOwned(PokemonRegion::Asia, "PMCG1", "001");
        asiaCard.id = 2;
        const auto regions =
            pokemonRegionsInCollection({westCard, asiaCard}, west, asia);
        REQUIRE(regions.size() == 2);
        CHECK(regions[0] == PokemonRegion::West);
        CHECK(regions[1] == PokemonRegion::Asia);
    }
}

TEST_SUITE("PokemonSetCatalogService") {
    TEST_CASE("save then load round-trips for west and asia paths") {
        InMemoryFileSystem fs;
        auto config = makeConfig(fs, "/data");
        PokemonSetCatalogService store{fs, config, [](Game) { return "pokemon"; }};

        CHECK_FALSE(store.exists(PokemonRegion::West));
        CHECK_FALSE(store.exists(PokemonRegion::Asia));
        CHECK(store.load(PokemonRegion::West).isErr());

        const auto west = westCatalog();
        const auto asia = asiaCatalog();
        REQUIRE(store.save(PokemonRegion::West, west).isOk());
        REQUIRE(store.save(PokemonRegion::Asia, asia).isOk());
        CHECK(store.exists(PokemonRegion::West));
        CHECK(store.exists(PokemonRegion::Asia));

        const auto loadedWest = store.load(PokemonRegion::West);
        REQUIRE(loadedWest.isOk());
        CHECK(loadedWest.value() == west);

        const auto loadedAsia = store.load(PokemonRegion::Asia);
        REQUIRE(loadedAsia.isOk());
        CHECK(loadedAsia.value() == asia);

        CHECK(fs.exists("/data/pokemon/set-catalog-west.json"));
        CHECK(fs.exists("/data/pokemon/set-catalog-asia.json"));
    }
}
