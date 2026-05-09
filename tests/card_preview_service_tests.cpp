#include <doctest/doctest.h>

#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"
#include "ccm/services/CardPreviewService.hpp"

#include <string>

using namespace ccm;

namespace {

class FakeSource final : public ICardPreviewSource {
public:
    std::string url = "https://example.com/preview.jpg";
    bool ok = true;
    std::string err = "boom";

    // Capture inputs so tests can assert routing.
    std::string lastName;
    std::string lastSetId;
    std::string lastSetNo;
    std::string detectLastName;
    std::string detectLastSetId;
    AutoDetectedPrint detectedPrint{"LOB-001", "Ultra Rare"};
    bool allowAutoDetect{true};

    Result<std::string> fetchImageUrl(std::string_view name,
                                      std::string_view setId,
                                      std::string_view setNo) override {
        lastName  = std::string(name);
        lastSetId = std::string(setId);
        lastSetNo = std::string(setNo);
        return ok ? Result<std::string>::ok(url)
                  : Result<std::string>::err(err);
    }

    Result<AutoDetectedPrint> detectFirstPrint(std::string_view name,
                                               std::string_view setId) override {
        detectLastName = std::string(name);
        detectLastSetId = std::string(setId);
        return Result<AutoDetectedPrint>::ok(detectedPrint);
    }

    [[nodiscard]] bool supportsAutoDetectPrint() const noexcept override {
        return allowAutoDetect;
    }
};

class FixedHttpClient final : public IHttpClient {
public:
    std::string lastUrl;
    std::string body;
    bool ok = true;
    std::string err = "offline";

    Result<std::string> get(std::string_view url) override {
        lastUrl = std::string(url);
        return ok ? Result<std::string>::ok(body)
                  : Result<std::string>::err(err);
    }
};

// Minimal IGameModule fake that exposes a configurable preview source.
class FakeGameModule final : public IGameModule {
public:
    Game gameId = Game::Magic;
    ICardPreviewSource* preview = nullptr;

    [[nodiscard]] Game        id() const noexcept override { return gameId; }
    [[nodiscard]] std::string dirName()     const override { return "fake"; }
    [[nodiscard]] std::string displayName() const override { return "Fake"; }

    ISetSource& setSource() override {
        // Tests in this file never call setSource(); a never-returned helper
        // would clutter the fake. Throwing keeps misuse loud.
        throw std::logic_error("FakeGameModule::setSource() not used in this test");
    }
    ICardPreviewSource* cardPreviewSource() noexcept override { return preview; }
};

}  // namespace

TEST_SUITE("CardPreviewService::fetchPreviewBytes") {
    TEST_CASE("happy path: routes through registered module then http GET") {
        FakeSource source;
        source.url = "https://example.com/img.png";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.body = std::string("\x89PNG\r\n\x1a\n", 8);  // arbitrary binary

        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.fetchPreviewBytes(Game::Magic, "Lightning Bolt", "lea", "");
        REQUIRE(out.isOk());
        CHECK(out.value() == http.body);
        CHECK(http.lastUrl == "https://example.com/img.png");
        CHECK(source.lastName  == "Lightning Bolt");
        CHECK(source.lastSetId == "lea");
        CHECK(source.lastSetNo.empty());
    }

    TEST_CASE("module returning nullptr preview source is skipped silently") {
        FakeGameModule module;
        module.gameId = Game::Pokemon;
        module.preview = nullptr;

        FixedHttpClient http;
        CardPreviewService svc{http};
        svc.registerModule(module);  // no-op

        const auto out = svc.fetchPreviewBytes(Game::Pokemon, "Pikachu", "sv3", "1");
        CHECK(out.isErr());
        CHECK(out.error().find("No preview source registered") != std::string::npos);
    }

    TEST_CASE("unregistered game returns an explicit error") {
        FixedHttpClient http;
        CardPreviewService svc{http};
        const auto out = svc.fetchPreviewBytes(Game::Pokemon, "Pikachu", "sv3", "1");
        CHECK(out.isErr());
        CHECK(out.error().find("No preview source registered") != std::string::npos);
    }

    TEST_CASE("source error propagates and http is not called") {
        FakeSource source;
        source.ok = false;
        source.err = "scryfall 404";
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.lastUrl = "<unset>";

        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.fetchPreviewBytes(Game::Magic, "X", "abc", "");
        CHECK(out.isErr());
        CHECK(out.error() == "scryfall 404");
        CHECK(http.lastUrl == "<unset>");
    }

    TEST_CASE("http GET error propagates") {
        FakeSource source;
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        http.ok = false;
        http.err = "net down";

        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.fetchPreviewBytes(Game::Magic, "X", "abc", "");
        CHECK(out.isErr());
        CHECK(out.error() == "net down");
    }
}

TEST_SUITE("CardPreviewService::detectFirstPrint") {
    TEST_CASE("routes auto-detect to registered source") {
        FakeSource source;
        source.detectedPrint = {"LOB-005", "Secret Rare"};
        FakeGameModule module;
        module.gameId = Game::YuGiOh;
        module.preview = &source;

        FixedHttpClient http;
        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.detectFirstPrint(Game::YuGiOh, "Dark Magician", "Legend of Blue Eyes White Dragon");
        REQUIRE(out.isOk());
        CHECK(out.value().setNo == "LOB-005");
        CHECK(out.value().rarity == "Secret Rare");
        CHECK(source.detectLastName == "Dark Magician");
        CHECK(source.detectLastSetId == "Legend of Blue Eyes White Dragon");
    }

    TEST_CASE("returns explicit error when game does not enable auto-detect") {
        FakeSource source;
        source.allowAutoDetect = false;
        FakeGameModule module;
        module.gameId = Game::Magic;
        module.preview = &source;

        FixedHttpClient http;
        CardPreviewService svc{http};
        svc.registerModule(module);

        const auto out = svc.detectFirstPrint(Game::Magic, "Any", "Any Set");
        CHECK(out.isErr());
        CHECK(out.error().find("not enabled") != std::string::npos);
    }
}
