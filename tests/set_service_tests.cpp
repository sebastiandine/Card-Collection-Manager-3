#include <doctest/doctest.h>

#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/ISetRepository.hpp"
#include "ccm/services/SetService.hpp"

#include <vector>

using namespace ccm;

namespace {

class FakeSetSource final : public ISetSource {
public:
    Result<std::vector<Set>> result = Result<std::vector<Set>>::err("not set");
    int calls = 0;
    Result<std::vector<Set>> fetchAll() override {
        ++calls;
        return result;
    }
};

class FakeGameModule final : public IGameModule {
public:
    FakeSetSource source;
    Game gameId;
    explicit FakeGameModule(Game id) : gameId(id) {}
    Game id() const noexcept override { return gameId; }
    std::string dirName()     const override {
        if (gameId == Game::Magic) return "magic";
        if (gameId == Game::Pokemon) return "pokemon";
        return "yugioh";
    }
    std::string displayName() const override { return dirName(); }
    ISetSource& setSource() override { return source; }
};

class InMemSetRepo final : public ISetRepository {
public:
    std::vector<Set> stored;
    bool hasStored = false;
    Result<std::vector<Set>> load(Game) override {
        if (!hasStored) return Result<std::vector<Set>>::err("no cache");
        return Result<std::vector<Set>>::ok(stored);
    }
    Result<void> save(Game, const std::vector<Set>& s) override {
        stored = s;
        hasStored = true;
        return Result<void>::ok();
    }
};

}  // namespace

TEST_SUITE("SetService") {
    TEST_CASE("updateSets fetches via the registered module and persists the result") {
        InMemSetRepo repo;
        SetService svc{repo};

        FakeGameModule magic{Game::Magic};
        magic.source.result = Result<std::vector<Set>>::ok({
            {"lea", "Alpha", "1993/08/05"},
        });
        svc.registerModule(&magic);

        const auto out = svc.updateSets(Game::Magic);
        REQUIRE(out.isOk());
        CHECK(out.value().size() == 1);
        CHECK(magic.source.calls == 1);

        // Cache is now warm.
        const auto cached = svc.getSets(Game::Magic);
        REQUIRE(cached.isOk());
        CHECK(cached.value() == out.value());
    }

    TEST_CASE("updateSets fails cleanly for unregistered games") {
        InMemSetRepo repo;
        SetService svc{repo};
        const auto out = svc.updateSets(Game::Pokemon);
        CHECK(out.isErr());
    }

    TEST_CASE("propagates upstream fetch errors without persisting") {
        InMemSetRepo repo;
        SetService svc{repo};

        FakeGameModule pokemon{Game::Pokemon};
        pokemon.source.result = Result<std::vector<Set>>::err("upstream is down");
        svc.registerModule(&pokemon);

        const auto out = svc.updateSets(Game::Pokemon);
        CHECK(out.isErr());
        CHECK_FALSE(repo.hasStored);
    }

    TEST_CASE("Pokemon module is routed independently of Magic") {
        // Both modules registered; updating Pokemon must hit the Pokemon
        // source and persist under the Pokemon Game key without disturbing
        // a previously cached Magic list.
        InMemSetRepo repo;
        SetService svc{repo};

        FakeGameModule magic{Game::Magic};
        magic.source.result = Result<std::vector<Set>>::ok({
            {"lea", "Alpha", "1993/08/05"},
        });
        FakeGameModule pokemon{Game::Pokemon};
        pokemon.source.result = Result<std::vector<Set>>::ok({
            {"base1", "Base", "1999/01/09"},
        });
        svc.registerModule(&magic);
        svc.registerModule(&pokemon);

        REQUIRE(svc.updateSets(Game::Magic).isOk());
        const auto poke = svc.updateSets(Game::Pokemon);
        REQUIRE(poke.isOk());
        REQUIRE(poke.value().size() == 1);
        CHECK(poke.value().front().id == "base1");
        CHECK(magic.source.calls   == 1);
        CHECK(pokemon.source.calls == 1);
    }

    TEST_CASE("YuGiOh module routes independently when all games are registered") {
        InMemSetRepo repo;
        SetService svc{repo};

        FakeGameModule magic{Game::Magic};
        magic.source.result = Result<std::vector<Set>>::ok({{"lea", "Alpha", "1993/08/05"}});
        FakeGameModule pokemon{Game::Pokemon};
        pokemon.source.result = Result<std::vector<Set>>::ok({{"base1", "Base", "1999/01/09"}});
        FakeGameModule yugioh{Game::YuGiOh};
        yugioh.source.result = Result<std::vector<Set>>::ok({{"LOB", "Legend of Blue Eyes", "2002/03/08"}});

        svc.registerModule(&magic);
        svc.registerModule(&pokemon);
        svc.registerModule(&yugioh);

        REQUIRE(svc.updateSets(Game::Magic).isOk());
        REQUIRE(svc.updateSets(Game::Pokemon).isOk());
        const auto ygo = svc.updateSets(Game::YuGiOh);
        REQUIRE(ygo.isOk());
        CHECK(ygo.value().front().id == "LOB");
        CHECK(magic.source.calls == 1);
        CHECK(pokemon.source.calls == 1);
        CHECK(yugioh.source.calls == 1);
    }
}
