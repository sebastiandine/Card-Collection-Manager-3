#include "ccm/domain/Configuration.hpp"
#include "ccm/domain/YuGiOhBandaiCard.hpp"
#include "ccm/domain/YuGiOhBandaiSetCatalog.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/YuGiOhBandaiSetCatalogService.hpp"
#include "ccm/services/YuGiOhBandaiSetCompletion.hpp"
#include "fakes/InMemoryFileSystem.hpp"

#include <nlohmann/json.hpp>

#include <doctest/doctest.h>

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

YuGiOhBandaiCard makeOwned(std::string setId, std::string setName, std::string setNo) {
    YuGiOhBandaiCard c;
    c.set = Set{std::move(setId), std::move(setName), "1998/09/01"};
    c.setNo = std::move(setNo);
    c.name = "Card";
    c.language = Language::Japanese;
    return c;
}

}  // namespace

TEST_SUITE("YuGiOhBandaiSetCompletion") {
    TEST_CASE("unique setNo within a pack; amount does not inflate") {
        YuGiOhBandaiSetCatalog catalog;
        YuGiOhBandaiSetCatalogPack pack;
        pack.setId = "ban1";
        pack.setName = "1st Generation";
        pack.cards.push_back({"9", "Blue-Eyes", "Super Rare"});
        pack.cards.push_back({"14", "Dark Magician", "Rare"});
        catalog.packs.push_back(pack);

        std::vector<YuGiOhBandaiCard> coll;
        auto a = makeOwned("ban1", "1st Generation", "14");
        a.amount = 5;
        coll.push_back(a);
        coll.push_back(makeOwned("ban1", "1st Generation", "014"));

        auto progress = computeYuGiOhBandaiSetCompletion(coll, catalog);
        REQUIRE(progress.size() == 1);
        CHECK(progress[0].ownedUnique == 1);
        CHECK(progress[0].total == 2);
        CHECK(progress[0].percent() == 50);
    }

    TEST_CASE("ownership on one pack does not complete another pack sharing setNo") {
        YuGiOhBandaiSetCatalog catalog;
        YuGiOhBandaiSetCatalogPack ban1;
        ban1.setId = "ban1";
        ban1.setName = "1st Generation";
        ban1.cards.push_back({"14", "Dark Magician", "Rare"});
        YuGiOhBandaiSetCatalogPack seal;
        seal.setId = "bansealdass";
        seal.setName = "Sealdass";
        seal.cards.push_back({"14", "Other", "Common"});
        catalog.packs.push_back(ban1);
        catalog.packs.push_back(seal);

        std::vector<YuGiOhBandaiCard> coll{makeOwned("ban1", "1st Generation", "14")};
        auto progress = computeYuGiOhBandaiSetCompletion(coll, catalog);
        REQUIRE(progress.size() == 1);
        CHECK(progress[0].setId == "ban1");
    }

    TEST_CASE("checklist marks owned rows") {
        YuGiOhBandaiSetCatalog catalog;
        YuGiOhBandaiSetCatalogPack pack;
        pack.setId = "ban1";
        pack.setName = "1st Generation";
        pack.cards.push_back({"9", "Blue-Eyes", "Super Rare"});
        pack.cards.push_back({"14", "Dark Magician", "Rare"});
        catalog.packs.push_back(pack);

        std::vector<YuGiOhBandaiCard> coll{makeOwned("ban1", "1st Generation", "14")};
        auto list = yuGiOhBandaiChecklistForSet(coll, catalog, "ban1");
        REQUIRE(list.size() == 2);
        CHECK(list[0].setNo == "14");
        CHECK(list[0].owned);
        CHECK(list[1].setNo == "9");
        CHECK_FALSE(list[1].owned);
    }

    TEST_CASE("empty setNo produces no progress rows") {
        YuGiOhBandaiSetCatalog catalog;
        YuGiOhBandaiSetCatalogPack pack;
        pack.setId = "ban1";
        pack.setName = "1st Generation";
        pack.cards.push_back({"14", "Dark Magician", "Rare"});
        catalog.packs.push_back(pack);

        YuGiOhBandaiCard missingNo = makeOwned("ban1", "1st Generation", "");
        YuGiOhBandaiCard whitespaceNo = makeOwned("ban1", "1st Generation", "   ");
        std::vector<YuGiOhBandaiCard> coll{missingNo, whitespaceNo};
        auto progress = computeYuGiOhBandaiSetCompletion(coll, catalog);
        CHECK(progress.empty());
    }

    TEST_CASE("set.id plus setNo against catalog pack yields a tile") {
        YuGiOhBandaiSetCatalog catalog;
        YuGiOhBandaiSetCatalogPack pack;
        pack.setId = "ban2";
        pack.setName = "2nd Generation";
        pack.cards.push_back({"47", "Time Wizard", "Super Rare"});
        pack.cards.push_back({"48", "Polymerization", "Super Rare"});
        catalog.packs.push_back(pack);

        std::vector<YuGiOhBandaiCard> coll{makeOwned("ban2", "2nd Generation", "047")};
        auto progress = computeYuGiOhBandaiSetCompletion(coll, catalog);
        REQUIRE(progress.size() == 1);
        CHECK(progress[0].setId == "ban2");
        CHECK(progress[0].setName == "2nd Generation");
        CHECK(progress[0].ownedUnique == 1);
        CHECK(progress[0].total == 2);
        CHECK(progress[0].percent() == 50);
    }
}

TEST_SUITE("YuGiOhBandaiSetCatalogService") {
    TEST_CASE("catalog service round-trips against InMemoryFileSystem") {
        InMemoryFileSystem fs;
        auto config = makeConfig(fs, "/data");
        YuGiOhBandaiSetCatalogService svc(fs, config, [](Game) { return "yugiohbandai"; });

        YuGiOhBandaiSetCatalog catalog;
        YuGiOhBandaiSetCatalogPack pack;
        pack.setId = "ban1";
        pack.setName = "1st Generation";
        pack.cards.push_back({"14", "Dark Magician", "Rare"});
        catalog.packs.push_back(pack);

        REQUIRE(svc.save(catalog));
        CHECK(svc.exists());
        auto loaded = svc.load();
        REQUIRE(loaded);
        CHECK(loaded.value() == catalog);
    }
}
