#include <doctest/doctest.h>

#include "ccm/domain/YuGiOhCard.hpp"
#include "ccm/domain/YuGiOhSetCatalog.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/YuGiOhSetCatalogService.hpp"
#include "ccm/services/YuGiOhSetCompletion.hpp"
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

YuGiOhCard makeOwned(std::string setId, std::string setName, std::string setNo) {
    YuGiOhCard c;
    c.id = 1;
    c.name = "Owned";
    c.set.id = std::move(setId);
    c.set.name = std::move(setName);
    c.setNo = std::move(setNo);
    return c;
}

YuGiOhSetCatalog sampleCatalog() {
    YuGiOhSetCatalog catalog;
    YuGiOhSetCatalogPack lob;
    lob.setId = "LOB";
    lob.setName = "Legend of Blue Eyes White Dragon";
    lob.cards = {
        {"LOB-001", "Blue-Eyes White Dragon"},
        {"LOB-EN005", "Dark Magician"},
        {"LOB-007", "Gaia The Fierce Knight"},
    };
    YuGiOhSetCatalogPack mrd;
    mrd.setId = "MRD";
    mrd.setName = "Metal Raiders";
    mrd.cards = {
        {"MRD-001", "Summoned Skull"},
        {"LOB-001", "Blue-Eyes White Dragon"},
    };
    catalog.packs.push_back(std::move(mrd));
    catalog.packs.push_back(std::move(lob));
    return catalog;
}

}  // namespace

TEST_SUITE("computeYuGiOhSetCompletion") {
    TEST_CASE("only packs with owned cards appear") {
        const auto catalog = sampleCatalog();
        std::vector<YuGiOhCard> collection{
            makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-001"),
        };
        const auto rows = computeYuGiOhSetCompletion(collection, catalog);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].setId == "LOB");
        CHECK(rows[0].ownedUnique == 1);
        CHECK(rows[0].total == 3);
        CHECK(rows[0].percent() == 33);
    }

    TEST_CASE("printing slot match treats LOB-005 and LOB-EN005 as one slot") {
        const auto catalog = sampleCatalog();
        YuGiOhCard a = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-005");
        a.amount = 4;
        YuGiOhCard b = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-EN005");
        b.id = 2;
        YuGiOhCard c = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-001");
        c.id = 3;
        const auto rows = computeYuGiOhSetCompletion({a, b, c}, catalog);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].ownedUnique == 2);
        CHECK(rows[0].total == 3);
        CHECK(rows[0].percent() == 66);
    }

    TEST_CASE("ownership on one pack does not complete another pack sharing setNo") {
        const auto catalog = sampleCatalog();
        std::vector<YuGiOhCard> collection{
            makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-001"),
        };
        const auto rows = computeYuGiOhSetCompletion(collection, catalog);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].setId == "LOB");
    }

    TEST_CASE("empty catalog yields no rows") {
        YuGiOhSetCatalog empty;
        std::vector<YuGiOhCard> collection{
            makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-001"),
        };
        CHECK(computeYuGiOhSetCompletion(collection, empty).empty());
    }

    TEST_CASE("owned set missing from catalog is skipped") {
        YuGiOhSetCatalog catalog;
        YuGiOhSetCatalogPack onlyMrd;
        onlyMrd.setId = "MRD";
        onlyMrd.setName = "Metal Raiders";
        onlyMrd.cards = {{"MRD-001", "Summoned Skull"}};
        catalog.packs.push_back(std::move(onlyMrd));

        std::vector<YuGiOhCard> collection{
            makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-001"),
        };
        CHECK(computeYuGiOhSetCompletion(collection, catalog).empty());
    }

    TEST_CASE("language filter hides packs with no cards in that language") {
        const auto catalog = sampleCatalog();
        YuGiOhCard en = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-001");
        en.language = Language::English;

        const auto allRows = computeYuGiOhSetCompletion({en}, catalog);
        REQUIRE(allRows.size() == 1);

        const auto deRows =
            computeYuGiOhSetCompletion({en}, catalog, Language::German);
        CHECK(deRows.empty());

        const auto enRows =
            computeYuGiOhSetCompletion({en}, catalog, Language::English);
        REQUIRE(enRows.size() == 1);
        CHECK(enRows[0].ownedUnique == 1);
    }

    TEST_CASE("same slot in two languages counts once aggregated; filter is exclusive") {
        const auto catalog = sampleCatalog();
        YuGiOhCard en = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-001");
        en.language = Language::English;
        YuGiOhCard de = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-001");
        de.id = 2;
        de.language = Language::German;

        const auto allRows = computeYuGiOhSetCompletion({en, de}, catalog);
        REQUIRE(allRows.size() == 1);
        CHECK(allRows[0].ownedUnique == 1);

        const auto enRows =
            computeYuGiOhSetCompletion({en, de}, catalog, Language::English);
        REQUIRE(enRows.size() == 1);
        CHECK(enRows[0].ownedUnique == 1);

        YuGiOhCard deOnly = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-005");
        deOnly.id = 3;
        deOnly.language = Language::German;
        const auto deRows =
            computeYuGiOhSetCompletion({en, de, deOnly}, catalog, Language::German);
        REQUIRE(deRows.size() == 1);
        CHECK(deRows[0].ownedUnique == 2);
    }
}

TEST_SUITE("yuGiOhChecklistForSet") {
    TEST_CASE("greys missing cards and marks owned ones") {
        const auto catalog = sampleCatalog();
        std::vector<YuGiOhCard> collection{
            makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-005"),
        };
        const auto list = yuGiOhChecklistForSet(collection, catalog, "LOB");
        REQUIRE(list.size() == 3);
        CHECK(list[0].setNo == "LOB-001");
        CHECK(list[0].owned == false);
        CHECK(list[1].setNo == "LOB-007");
        CHECK(list[1].owned == false);
        CHECK(list[2].setNo == "LOB-EN005");
        CHECK(list[2].owned == true);
    }

    TEST_CASE("unknown set returns empty") {
        const auto catalog = sampleCatalog();
        CHECK(yuGiOhChecklistForSet({}, catalog, "missing").empty());
    }

    TEST_CASE("owned flags respect language filter") {
        const auto catalog = sampleCatalog();
        YuGiOhCard en = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-005");
        en.language = Language::English;

        const auto filtered =
            yuGiOhChecklistForSet({en}, catalog, "LOB", Language::German);
        REQUIRE(filtered.size() == 3);
        CHECK(filtered[0].owned == false);
        CHECK(filtered[1].owned == false);
        CHECK(filtered[2].owned == false);

        const auto english =
            yuGiOhChecklistForSet({en}, catalog, "LOB", Language::English);
        REQUIRE(english.size() == 3);
        CHECK(english[2].owned == true);
    }
}

TEST_SUITE("yuGiOhLanguagesInCollection") {
    TEST_CASE("empty collection yields empty") {
        CHECK(yuGiOhLanguagesInCollection({}).empty());
    }

    TEST_CASE("returns distinct languages in allLanguages order") {
        YuGiOhCard jp = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-001");
        jp.language = Language::Japanese;
        YuGiOhCard en = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-005");
        en.id = 2;
        en.language = Language::English;
        YuGiOhCard enDup = makeOwned("MRD", "Metal Raiders", "MRD-001");
        enDup.id = 3;
        enDup.language = Language::English;
        YuGiOhCard de = makeOwned("LOB", "Legend of Blue Eyes White Dragon", "LOB-007");
        de.id = 4;
        de.language = Language::German;

        const auto langs = yuGiOhLanguagesInCollection({jp, en, enDup, de});
        REQUIRE(langs.size() == 3);
        CHECK(langs[0] == Language::English);
        CHECK(langs[1] == Language::German);
        CHECK(langs[2] == Language::Japanese);
    }
}

TEST_SUITE("YuGiOhSetCatalogService") {
    TEST_CASE("save then load round-trips") {
        InMemoryFileSystem fs;
        auto config = makeConfig(fs, "/data");
        YuGiOhSetCatalogService store{fs, config, [](Game) { return "yugioh"; }};

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
