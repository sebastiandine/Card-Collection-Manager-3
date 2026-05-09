#include <doctest/doctest.h>

#include "ccm/domain/MagicCard.hpp"
#include "ccm/ports/ICollectionRepository.hpp"
#include "ccm/ports/IImageStore.hpp"
#include "ccm/services/CollectionService.hpp"

#include <map>
#include <string>
#include <vector>

using namespace ccm;

namespace {

class InMemoryRepo final : public ICollectionRepository<MagicCard> {
public:
    Map storage;

    Result<Map> load(Game) override { return Result<Map>::ok(storage); }
    Result<void> save(Game, const Map& m) override {
        storage = m;
        return Result<void>::ok();
    }
};

class StubImageStore final : public IImageStore {
public:
    std::vector<std::pair<Game, std::string>> removed;

    Result<std::string> copyIn(Game, const std::filesystem::path&, const std::string& n) override {
        return Result<std::string>::ok(n);
    }
    Result<void> remove(Game g, const std::string& n) override {
        removed.emplace_back(g, n);
        return Result<void>::ok();
    }
    std::filesystem::path resolvePath(Game, const std::string& n) const override {
        return std::filesystem::path(n);
    }
};

MagicCard makeCard(const std::string& name, std::vector<std::string> imgs = {}) {
    MagicCard c;
    c.name = name;
    c.set = Set{"alp", "Alpha", "1993/08/05"};
    c.images = std::move(imgs);
    return c;
}

}  // namespace

TEST_SUITE("CollectionService<MagicCard>") {
    TEST_CASE("nextId on empty map is 0, then strictly increments") {
        InMemoryRepo repo;
        StubImageStore store;
        CollectionService<MagicCard> svc{repo, store};

        const auto id0 = svc.add(Game::Magic, makeCard("A"));
        REQUIRE(id0.isOk());
        CHECK(id0.value() == 0);

        const auto id1 = svc.add(Game::Magic, makeCard("B"));
        REQUIRE(id1.isOk());
        CHECK(id1.value() == 1);

        const auto listed = svc.list(Game::Magic);
        REQUIRE(listed.isOk());
        CHECK(listed.value().size() == 2);
    }

    TEST_CASE("update modifies an existing card and is rejected for unknown ids") {
        InMemoryRepo repo;
        StubImageStore store;
        CollectionService<MagicCard> svc{repo, store};

        const auto id = svc.add(Game::Magic, makeCard("Initial")).value();

        MagicCard updated = makeCard("Renamed");
        updated.id = id;
        CHECK(svc.update(Game::Magic, updated).isOk());

        const auto found = svc.findById(Game::Magic, id);
        REQUIRE(found.isOk());
        REQUIRE(found.value().has_value());
        CHECK(found.value()->name == "Renamed");

        MagicCard ghost = makeCard("Ghost");
        ghost.id = 999;
        CHECK(svc.update(Game::Magic, ghost).isErr());
    }

    TEST_CASE("remove deletes images and the entry") {
        InMemoryRepo repo;
        StubImageStore store;
        CollectionService<MagicCard> svc{repo, store};

        const auto id = svc.add(
            Game::Magic, makeCard("With Images", {"a.png", "b.png"})).value();

        REQUIRE(svc.remove(Game::Magic, id).isOk());

        // Both images should have been requested for deletion.
        REQUIRE(store.removed.size() == 2);
        CHECK(store.removed[0].second == "a.png");
        CHECK(store.removed[1].second == "b.png");

        const auto listed = svc.list(Game::Magic);
        REQUIRE(listed.isOk());
        CHECK(listed.value().empty());
    }

    TEST_CASE("remove of unknown id returns an error") {
        InMemoryRepo repo;
        StubImageStore store;
        CollectionService<MagicCard> svc{repo, store};

        CHECK(svc.remove(Game::Magic, 12345).isErr());
    }
}
