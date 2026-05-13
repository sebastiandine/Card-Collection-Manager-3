#include <doctest/doctest.h>

#include "ccm/domain/MagicCard.hpp"
#include "ccm/infra/JsonCollectionRepository.hpp"
#include "ccm/services/ConfigService.hpp"

#include "fakes/InMemoryFileSystem.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>

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

class FailingCollectionFs final : public IFileSystem {
public:
    bool existsValue{true};
    bool ensureOk{true};
    bool writeOk{true};
    bool readOk{true};
    std::string readPayload{"{}"};

    [[nodiscard]] bool exists(const std::filesystem::path&) const override { return existsValue; }
    [[nodiscard]] bool isDirectory(const std::filesystem::path&) const override { return true; }
    Result<void> ensureDirectory(const std::filesystem::path&) override {
        if (!ensureOk) return Result<void>::err("ensure failed");
        return Result<void>::ok();
    }
    Result<std::string> readText(const std::filesystem::path&) override {
        if (!readOk) return Result<std::string>::err("read failed");
        return Result<std::string>::ok(readPayload);
    }
    Result<void> writeText(const std::filesystem::path&, std::string_view) override {
        if (!writeOk) return Result<void>::err("write failed");
        return Result<void>::ok();
    }
    Result<void> copyFile(const std::filesystem::path&, const std::filesystem::path&, bool) override {
        return Result<void>::ok();
    }
    Result<void> remove(const std::filesystem::path&) override { return Result<void>::ok(); }
    Result<std::vector<std::filesystem::path>> listDirectory(const std::filesystem::path&) override {
        return Result<std::vector<std::filesystem::path>>::ok({});
    }
};

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

    TEST_CASE("load returns parse error for non-object root") {
        InMemoryFileSystem fs;
        auto cfg = makeConfig(fs, "/data");
        JsonCollectionRepository<MagicCard> repo{fs, cfg, magicDir};
        fs.writeText("/data/magic/collection.json", R"(["not","an","object"])");

        const auto loaded = repo.load(Game::Magic);
        REQUIRE(loaded.isErr());
        CHECK(loaded.error().find("JSON parse error:") != std::string::npos);
    }

    TEST_CASE("load returns parse error for non-numeric object keys") {
        InMemoryFileSystem fs;
        auto cfg = makeConfig(fs, "/data");
        JsonCollectionRepository<MagicCard> repo{fs, cfg, magicDir};
        fs.writeText("/data/magic/collection.json", R"({"abc":{"id":1}})");

        const auto loaded = repo.load(Game::Magic);
        REQUIRE(loaded.isErr());
        CHECK(loaded.error().find("JSON parse error:") != std::string::npos);
    }

    TEST_CASE("save and initialize-on-load propagate ensureDirectory/write errors") {
        InMemoryFileSystem configFs;
        auto cfg = makeConfig(configFs, "/data");
        FailingCollectionFs fs;
        JsonCollectionRepository<MagicCard> repo{fs, cfg, magicDir};

        fs.ensureOk = false;
        const auto saveEnsureFail = repo.save(Game::Magic, {});
        REQUIRE(saveEnsureFail.isErr());
        CHECK(saveEnsureFail.error() == "ensure failed");

        fs.ensureOk = true;
        fs.writeOk = false;
        const auto saveWriteFail = repo.save(Game::Magic, {});
        REQUIRE(saveWriteFail.isErr());
        CHECK(saveWriteFail.error() == "write failed");

        fs.existsValue = false;
        const auto loadCreateFail = repo.load(Game::Magic);
        REQUIRE(loadCreateFail.isErr());
        CHECK(loadCreateFail.error() == "write failed");
    }

    TEST_CASE("load returns read error when collection exists but read fails") {
        InMemoryFileSystem configFs;
        auto cfg = makeConfig(configFs, "/data");
        FailingCollectionFs fs;
        JsonCollectionRepository<MagicCard> repo{fs, cfg, magicDir};
        fs.existsValue = true;
        fs.readOk = false;

        const auto loaded = repo.load(Game::Magic);
        REQUIRE(loaded.isErr());
        CHECK(loaded.error() == "read failed");
    }

    TEST_CASE("load returns parse error when card object does not deserialize") {
        InMemoryFileSystem fs;
        auto cfg = makeConfig(fs, "/data");
        JsonCollectionRepository<MagicCard> repo{fs, cfg, magicDir};
        fs.writeText("/data/magic/collection.json", R"({"0":{"id":"not-a-number"}})");

        const auto loaded = repo.load(Game::Magic);
        REQUIRE(loaded.isErr());
        CHECK(loaded.error().find("JSON parse error:") != std::string::npos);
    }
}
