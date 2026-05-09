#include <doctest/doctest.h>

#include "ccm/infra/JsonSetRepository.hpp"
#include "ccm/services/ConfigService.hpp"

#include "fakes/InMemoryFileSystem.hpp"

#include <nlohmann/json.hpp>

using namespace ccm;
using ccm::testing::InMemoryFileSystem;

namespace {
std::string dirNameFn(Game g) { return g == Game::Magic ? "magic" : "pokemon"; }

ConfigService makeConfig(InMemoryFileSystem& fs, const std::string& dataDir) {
    Configuration c;
    c.dataStorage = dataDir;
    c.defaultGame = Game::Magic;
    fs.writeText("/app/config.json", nlohmann::json(c).dump());
    ConfigService cfg{fs, "/app/config.json", dataDir};
    cfg.initialize();
    return cfg;
}
}  // namespace

TEST_SUITE("JsonSetRepository") {
    TEST_CASE("save then load returns identical sets") {
        InMemoryFileSystem fs;
        auto cfg = makeConfig(fs, "/data");
        JsonSetRepository repo{fs, cfg, dirNameFn};

        const std::vector<Set> sets = {
            {"lea", "Limited Edition Alpha", "1993/08/05"},
            {"leb", "Limited Edition Beta",  "1993/10/04"},
        };
        REQUIRE(repo.save(Game::Magic, sets).isOk());

        const auto loaded = repo.load(Game::Magic);
        REQUIRE(loaded.isOk());
        CHECK(loaded.value() == sets);
    }

    TEST_CASE("load before any save reports a clear error") {
        InMemoryFileSystem fs;
        auto cfg = makeConfig(fs, "/data");
        JsonSetRepository repo{fs, cfg, dirNameFn};

        const auto loaded = repo.load(Game::Pokemon);
        CHECK(loaded.isErr());
    }
}
