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
    bool failLoad{false};
    bool failSave{false};

    Result<Map> load(Game) override {
        if (failLoad) return Result<Map>::err("load failed");
        return Result<Map>::ok(storage);
    }
    Result<void> save(Game, const Map& m) override {
        if (failSave) return Result<void>::err("save failed");
        storage = m;
        return Result<void>::ok();
    }
};

class StubImageStore final : public IImageStore {
public:
    std::vector<std::pair<Game, std::string>> removed;
    bool failRemove{false};

    Result<std::string> copyIn(Game, const std::filesystem::path&, const std::string& n) override {
        return Result<std::string>::ok(n);
    }
    Result<void> remove(Game g, const std::string& n) override {
        removed.emplace_back(g, n);
        if (failRemove) return Result<void>::err("remove failed for " + n);
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

    TEST_CASE("load errors are propagated by list and findById") {
        InMemoryRepo repo;
        repo.failLoad = true;
        StubImageStore store;
        CollectionService<MagicCard> svc{repo, store};

        const auto listed = svc.list(Game::Magic);
        REQUIRE(listed.isErr());
        CHECK(listed.error() == "load failed");

        const auto found = svc.findById(Game::Magic, 1);
        REQUIRE(found.isErr());
        CHECK(found.error() == "load failed");
    }

    TEST_CASE("save errors are propagated by add and update") {
        InMemoryRepo repo;
        repo.failSave = true;
        StubImageStore store;
        CollectionService<MagicCard> svc{repo, store};

        const auto addRes = svc.add(Game::Magic, makeCard("A"));
        REQUIRE(addRes.isErr());
        CHECK(addRes.error() == "save failed");

        repo.failSave = false;
        const auto id = svc.add(Game::Magic, makeCard("B"));
        REQUIRE(id.isOk());

        repo.failSave = true;
        MagicCard updated = makeCard("Renamed");
        updated.id = id.value();
        const auto updateRes = svc.update(Game::Magic, updated);
        REQUIRE(updateRes.isErr());
        CHECK(updateRes.error() == "save failed");
    }

    TEST_CASE("findById returns nullopt for missing id") {
        InMemoryRepo repo;
        StubImageStore store;
        CollectionService<MagicCard> svc{repo, store};

        const auto out = svc.findById(Game::Magic, 99);
        REQUIRE(out.isOk());
        CHECK_FALSE(out.value().has_value());
    }

    TEST_CASE("remove reports image cleanup issues but still removes card") {
        InMemoryRepo repo;
        StubImageStore store;
        store.failRemove = true;
        CollectionService<MagicCard> svc{repo, store};

        const auto id = svc.add(
            Game::Magic, makeCard("With Images", {"a.png", "b.png"}));
        REQUIRE(id.isOk());

        const auto removed = svc.remove(Game::Magic, id.value());
        REQUIRE(removed.isErr());
        CHECK(removed.error().find("Card removed but image cleanup had issues:") != std::string::npos);
        CHECK(removed.error().find("remove failed for a.png") != std::string::npos);
        CHECK(removed.error().find("remove failed for b.png") != std::string::npos);

        const auto listed = svc.list(Game::Magic);
        REQUIRE(listed.isOk());
        CHECK(listed.value().empty());
    }
}
