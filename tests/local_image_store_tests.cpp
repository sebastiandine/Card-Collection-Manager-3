#include <doctest/doctest.h>

#include "ccm/infra/LocalImageStore.hpp"
#include "ccm/services/ConfigService.hpp"

#include "fakes/InMemoryFileSystem.hpp"

#include <filesystem>

using namespace ccm;
using ccm::testing::InMemoryFileSystem;

namespace {

std::string dirNameForGame(Game g) {
    switch (g) {
        case Game::Magic: return "magic";
        case Game::Pokemon: return "pokemon";
        case Game::YuGiOh: return "yugioh";
    }
    return "magic";
}

}  // namespace

TEST_SUITE("LocalImageStore") {
    TEST_CASE("copyIn creates images directory and preserves source extension") {
        InMemoryFileSystem mem;
        ConfigService               cfg{mem, "/app/config.json", "/coll"};
        REQUIRE(cfg.initialize().isOk());
        LocalImageStore store(mem, cfg, dirNameForGame);

        REQUIRE(mem.writeText("/incoming/card.PNG", "img-bytes").isOk());

        auto r = store.copyIn(Game::Magic, "/incoming/card.PNG", "id001");
        REQUIRE(r.isOk());
        CHECK(r.value() == "id001.PNG");

        const std::filesystem::path dest =
            std::filesystem::path(cfg.current().dataStorage) / "magic" / "images" / "id001.PNG";
        REQUIRE(mem.exists(dest));
        auto body = mem.readText(dest);
        REQUIRE(body.isOk());
        CHECK(body.value() == "img-bytes");
    }

    TEST_CASE("copyIn errors when source file is missing") {
        InMemoryFileSystem mem;
        ConfigService      cfg{mem, "/app/config.json", "/coll"};
        REQUIRE(cfg.initialize().isOk());
        LocalImageStore store(mem, cfg, dirNameForGame);

        auto r = store.copyIn(Game::Pokemon, "/nope/missing.jpg", "x");
        REQUIRE(r.isErr());
    }

    TEST_CASE("remove deletes an existing image") {
        InMemoryFileSystem mem;
        ConfigService      cfg{mem, "/app/config.json", "/coll"};
        REQUIRE(cfg.initialize().isOk());
        LocalImageStore store(mem, cfg, dirNameForGame);

        const std::filesystem::path imagePath =
            std::filesystem::path(cfg.current().dataStorage) / "magic" / "images" / "a.png";
        REQUIRE(mem.ensureDirectory(imagePath.parent_path()).isOk());
        REQUIRE(mem.writeText(imagePath, "x").isOk());

        REQUIRE(store.remove(Game::Magic, "a.png").isOk());
        CHECK_FALSE(mem.exists(imagePath));
    }

    TEST_CASE("remove succeeds when file is already absent") {
        InMemoryFileSystem mem;
        ConfigService      cfg{mem, "/app/config.json", "/coll"};
        REQUIRE(cfg.initialize().isOk());
        LocalImageStore store(mem, cfg, dirNameForGame);

        REQUIRE(store.remove(Game::YuGiOh, "ghost.bin").isOk());
    }

    TEST_CASE("resolvePath joins data storage game images and filename") {
        InMemoryFileSystem mem;
        ConfigService      cfg{mem, "/app/config.json", "/coll"};
        REQUIRE(cfg.initialize().isOk());
        LocalImageStore store(mem, cfg, dirNameForGame);

        const std::filesystem::path got = store.resolvePath(Game::Pokemon, "pic.jpg");
        CHECK(got.generic_string() == "/coll/pokemon/images/pic.jpg");
    }
}
