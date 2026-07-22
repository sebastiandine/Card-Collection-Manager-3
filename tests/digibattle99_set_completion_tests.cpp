#include <doctest/doctest.h>

#include "ccm/domain/DigiBattle99Card.hpp"
#include "ccm/domain/DigiBattle99SetCatalog.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/DigiBattle99SetCatalogService.hpp"
#include "ccm/services/DigiBattle99SetCompletion.hpp"
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

DigiBattle99Card makeOwned(std::string setId, std::string setName, std::string setNo) {
    DigiBattle99Card c;
    c.id = 1;
    c.name = "Owned";
    c.set.id = std::move(setId);
    c.set.name = std::move(setName);
    c.setNo = std::move(setNo);
    return c;
}

DigiBattle99SetCatalog sampleCatalog() {
    DigiBattle99SetCatalog catalog;
    DigiBattle99SetCatalogPack starter;
    starter.setId = "series-1-starter-set";
    starter.setName = "Series 1 Starter Set";
    starter.cards = {
        {"ST-01", "Agumon"},
        {"ST-02", "Greymon"},
        {"ST-03", "Gabumon"},
    };
    DigiBattle99SetCatalogPack booster;
    booster.setId = "series-1-booster-pack";
    booster.setName = "Series 1 Booster Pack";
    booster.cards = {
        {"ST-01", "Agumon"},
        {"BO-01", "MetalGreymon"},
    };
    catalog.packs.push_back(std::move(booster));
    catalog.packs.push_back(std::move(starter));
    return catalog;
}

}  // namespace

TEST_SUITE("computeDigiBattle99SetCompletion") {
    TEST_CASE("only packs with owned cards appear") {
        const auto catalog = sampleCatalog();
        std::vector<DigiBattle99Card> collection{
            makeOwned("series-1-starter-set", "Series 1 Starter Set", "ST-01"),
        };
        const auto rows = computeDigiBattle99SetCompletion(collection, catalog);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].setId == "series-1-starter-set");
        CHECK(rows[0].ownedUnique == 1);
        CHECK(rows[0].total == 3);
        CHECK(rows[0].percent() == 33);
    }

    TEST_CASE("unique setNo within a pack; amount does not inflate") {
        const auto catalog = sampleCatalog();
        DigiBattle99Card a = makeOwned("series-1-starter-set", "Series 1 Starter Set", "st-01");
        a.amount = 4;
        DigiBattle99Card b = makeOwned("series-1-starter-set", "Series 1 Starter Set", "ST-01");
        b.id = 2;
        DigiBattle99Card c = makeOwned("series-1-starter-set", "Series 1 Starter Set", "ST-02");
        c.id = 3;
        const auto rows =
            computeDigiBattle99SetCompletion({a, b, c}, catalog);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].ownedUnique == 2);
        CHECK(rows[0].total == 3);
        CHECK(rows[0].percent() == 66);
    }

    TEST_CASE("ownership on one pack does not complete another pack sharing setNo") {
        const auto catalog = sampleCatalog();
        std::vector<DigiBattle99Card> collection{
            makeOwned("series-1-starter-set", "Series 1 Starter Set", "ST-01"),
        };
        const auto rows = computeDigiBattle99SetCompletion(collection, catalog);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].setId == "series-1-starter-set");
    }

    TEST_CASE("empty catalog yields no rows") {
        DigiBattle99SetCatalog empty;
        std::vector<DigiBattle99Card> collection{
            makeOwned("series-1-starter-set", "Series 1 Starter Set", "ST-01"),
        };
        CHECK(computeDigiBattle99SetCompletion(collection, empty).empty());
    }

    TEST_CASE("owned set missing from catalog is skipped") {
        DigiBattle99SetCatalog catalog;
        DigiBattle99SetCatalogPack onlyBooster;
        onlyBooster.setId = "series-1-booster-pack";
        onlyBooster.setName = "Series 1 Booster Pack";
        onlyBooster.cards = {{"BO-01", "MetalGreymon"}};
        catalog.packs.push_back(std::move(onlyBooster));

        std::vector<DigiBattle99Card> collection{
            makeOwned("series-1-starter-set", "Series 1 Starter Set", "ST-01"),
        };
        CHECK(computeDigiBattle99SetCompletion(collection, catalog).empty());
    }
}

TEST_SUITE("digiBattle99ChecklistForSet") {
    TEST_CASE("greys missing cards and marks owned ones") {
        const auto catalog = sampleCatalog();
        std::vector<DigiBattle99Card> collection{
            makeOwned("series-1-starter-set", "Series 1 Starter Set", "ST-02"),
        };
        const auto list =
            digiBattle99ChecklistForSet(collection, catalog, "series-1-starter-set");
        REQUIRE(list.size() == 3);
        CHECK(list[0].setNo == "ST-01");
        CHECK(list[0].owned == false);
        CHECK(list[1].setNo == "ST-02");
        CHECK(list[1].owned == true);
        CHECK(list[2].setNo == "ST-03");
        CHECK(list[2].owned == false);
    }

    TEST_CASE("unknown set returns empty") {
        const auto catalog = sampleCatalog();
        CHECK(digiBattle99ChecklistForSet({}, catalog, "missing").empty());
    }
}

TEST_SUITE("DigiBattle99SetCatalogService") {
    TEST_CASE("save then load round-trips") {
        InMemoryFileSystem fs;
        auto config = makeConfig(fs, "/data");
        DigiBattle99SetCatalogService store{fs, config, [](Game) { return "digibattle99"; }};

        CHECK_FALSE(store.exists());
        CHECK(store.load().isErr());

        const auto catalog = sampleCatalog();
        REQUIRE(store.save(catalog).isOk());
        CHECK(store.exists());

        const auto loaded = store.load();
        REQUIRE(loaded.isOk());
        CHECK(loaded.value() == catalog);
    }
}
