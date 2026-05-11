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

class FailingSetFs final : public IFileSystem {
public:
    bool ensureOk{true};
    bool writeOk{true};
    bool readOk{true};
    std::string readPayload{"[]"};
    std::filesystem::path lastWritePath;
    std::string lastWriteBody;

    [[nodiscard]] bool exists(const std::filesystem::path&) const override { return true; }
    [[nodiscard]] bool isDirectory(const std::filesystem::path&) const override { return true; }

    Result<void> ensureDirectory(const std::filesystem::path&) override {
        if (!ensureOk) return Result<void>::err("ensure failed");
        return Result<void>::ok();
    }
    Result<std::string> readText(const std::filesystem::path&) override {
        if (!readOk) return Result<std::string>::err("read failed");
        return Result<std::string>::ok(readPayload);
    }
    Result<void> writeText(const std::filesystem::path& p, std::string_view contents) override {
        if (!writeOk) return Result<void>::err("write failed");
        lastWritePath = p;
        lastWriteBody = std::string(contents);
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

    TEST_CASE("load propagates read errors from filesystem") {
        InMemoryFileSystem configFs;
        auto cfg = makeConfig(configFs, "/data");
        FailingSetFs fs;
        fs.readOk = false;
        JsonSetRepository repo{fs, cfg, dirNameFn};

        const auto loaded = repo.load(Game::Magic);
        REQUIRE(loaded.isErr());
        CHECK(loaded.error() == "read failed");
    }

    TEST_CASE("load reports parse error for malformed sets.json") {
        InMemoryFileSystem configFs;
        auto cfg = makeConfig(configFs, "/data");
        FailingSetFs fs;
        fs.readPayload = "{bad json";
        JsonSetRepository repo{fs, cfg, dirNameFn};

        const auto loaded = repo.load(Game::Magic);
        REQUIRE(loaded.isErr());
        CHECK(loaded.error().find("sets.json parse error:") != std::string::npos);
    }

    TEST_CASE("save propagates ensureDirectory and writeText failures") {
        InMemoryFileSystem configFs;
        auto cfg = makeConfig(configFs, "/data");
        FailingSetFs fs;
        JsonSetRepository repo{fs, cfg, dirNameFn};

        const std::vector<Set> sets = {{"lea", "Limited Edition Alpha", "1993/08/05"}};

        fs.ensureOk = false;
        const auto ensureFail = repo.save(Game::Magic, sets);
        REQUIRE(ensureFail.isErr());
        CHECK(ensureFail.error() == "ensure failed");

        fs.ensureOk = true;
        fs.writeOk = false;
        const auto writeFail = repo.save(Game::Magic, sets);
        REQUIRE(writeFail.isErr());
        CHECK(writeFail.error() == "write failed");
    }
}
