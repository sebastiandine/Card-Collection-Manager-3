#include <doctest/doctest.h>

#include "ccm/services/ConfigService.hpp"

#include "fakes/InMemoryFileSystem.hpp"

#include <nlohmann/json.hpp>

using namespace ccm;
using ccm::testing::InMemoryFileSystem;

TEST_SUITE("ConfigService") {
    TEST_CASE("missing file is created with defaults") {
        InMemoryFileSystem fs;
        ConfigService svc{fs, "/app/config.json", "/data"};

        REQUIRE(svc.initialize().isOk());
        CHECK(svc.current().dataStorage == "/data");
        CHECK(svc.current().defaultGame == Game::Magic);
        CHECK(svc.current().theme == Theme::Light);

        const auto& written = fs.files();
        REQUIRE(written.count("/app/config.json"));
        const auto j = nlohmann::json::parse(written.at("/app/config.json"));
        CHECK(j.at("dataStorage") == "/data");
        CHECK(j.at("defaultGame") == "Magic");
        CHECK(j.at("theme") == "Light");
    }

    TEST_CASE("existing file is loaded verbatim") {
        InMemoryFileSystem fs;
        REQUIRE(fs.writeText("/app/config.json",
            R"({"dataStorage":"/somewhere","defaultGame":"Pokemon","theme":"Dark"})").isOk());

        ConfigService svc{fs, "/app/config.json", "/default"};
        REQUIRE(svc.initialize().isOk());
        CHECK(svc.current().dataStorage == "/somewhere");
        CHECK(svc.current().defaultGame == Game::Pokemon);
        CHECK(svc.current().theme == Theme::Dark);
    }

    TEST_CASE("store updates both the live config and the file") {
        InMemoryFileSystem fs;
        ConfigService svc{fs, "/app/config.json", "/data"};
        REQUIRE(svc.initialize().isOk());

        Configuration next;
        next.dataStorage = "/new/place";
        next.defaultGame = Game::Pokemon;
        next.theme = Theme::Dark;
        REQUIRE(svc.store(next).isOk());

        CHECK(svc.current() == next);
        const auto j = nlohmann::json::parse(fs.files().at("/app/config.json"));
        CHECK(j.at("dataStorage") == "/new/place");
        CHECK(j.at("defaultGame") == "Pokemon");
        CHECK(j.at("theme") == "Dark");
    }

    TEST_CASE("missing theme field defaults to light for compatibility") {
        InMemoryFileSystem fs;
        REQUIRE(fs.writeText("/app/config.json",
            R"({"dataStorage":"/somewhere","defaultGame":"Pokemon"})").isOk());

        ConfigService svc{fs, "/app/config.json", "/default"};
        REQUIRE(svc.initialize().isOk());
        CHECK(svc.current().theme == Theme::Light);
    }

    TEST_CASE("malformed JSON surfaces a clear error") {
        InMemoryFileSystem fs;
        REQUIRE(fs.writeText("/app/config.json", "not-json").isOk());
        ConfigService svc{fs, "/app/config.json", "/default"};
        const auto r = svc.initialize();
        REQUIRE(r.isErr());
        CHECK(r.error().find("config.json parse error") != std::string::npos);
    }
}
