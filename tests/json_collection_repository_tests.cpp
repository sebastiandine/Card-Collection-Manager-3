#include <doctest/doctest.h>

#include "ccm/domain/MagicCard.hpp"
#include "ccm/infra/JsonCollectionRepository.hpp"
#include "ccm/services/ConfigService.hpp"

#include "fakes/InMemoryFileSystem.hpp"

#include <nlohmann/json.hpp>

using namespace ccm;
using ccm::testing::InMemoryFileSystem;

namespace {

ConfigService makeConfig(InMemoryFileSystem& fs, const std::string& dataDir) {
    // Pre-write a config so initialize() doesn't reset our data directory.
    Configuration c;
    c.dataStorage = dataDir;
    c.defaultGame = Game::Magic;
    const nlohmann::json j = c;
    fs.writeText("/app/config.json", j.dump());
    ConfigService cfg{fs, "/app/config.json", dataDir};
    cfg.initialize();
    return cfg;
}

std::string magicDir(Game g) { return g == Game::Magic ? "magic" : "pokemon"; }

}  // namespace

TEST_SUITE("JsonCollectionRepository<MagicCard>") {
    TEST_CASE("missing collection.json is created on first load") {
        InMemoryFileSystem fs;
        auto cfg = makeConfig(fs, "/data");
        JsonCollectionRepository<MagicCard> repo{fs, cfg, magicDir};

        const auto loaded = repo.load(Game::Magic);
        REQUIRE(loaded.isOk());
        CHECK(loaded.value().empty());
        CHECK(fs.files().count("/data/magic/collection.json") == 1);
    }

    TEST_CASE("save/load round-trip preserves all card fields") {
        InMemoryFileSystem fs;
        auto cfg = makeConfig(fs, "/data");
        JsonCollectionRepository<MagicCard> repo{fs, cfg, magicDir};

        MagicCard c;
        c.id = 5;
        c.amount = 2;
        c.name = "Black Lotus";
        c.set = Set{"lea", "Limited Edition Alpha", "1993/08/05"};
        c.note = "tournament-illegal";
        c.images = {"lea+BlackLotus+0.png"};
        c.language = Language::English;
        c.condition = Condition::Mint;
        c.foil = false;
        c.signed_ = true;
        c.altered = false;

        std::map<std::uint32_t, MagicCard> m{{c.id, c}};
        REQUIRE(repo.save(Game::Magic, m).isOk());

        const auto loaded = repo.load(Game::Magic);
        REQUIRE(loaded.isOk());
        REQUIRE(loaded.value().count(5) == 1);
        CHECK(loaded.value().at(5) == c);
    }

    TEST_CASE("on-disk JSON is keyed by stringified card id") {
        InMemoryFileSystem fs;
        auto cfg = makeConfig(fs, "/data");
        JsonCollectionRepository<MagicCard> repo{fs, cfg, magicDir};

        MagicCard c;
        c.id = 17;
        c.name = "X";
        c.set = Set{"x", "X", "2024/01/01"};
        std::map<std::uint32_t, MagicCard> m{{c.id, c}};
        REQUIRE(repo.save(Game::Magic, m).isOk());

        const auto& contents = fs.files().at("/data/magic/collection.json");
        const auto j = nlohmann::json::parse(contents);
        REQUIRE(j.contains("17"));
        CHECK(j.at("17").at("id") == 17);
    }
}
