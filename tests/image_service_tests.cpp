#include <doctest/doctest.h>

#include "ccm/domain/Enums.hpp"
#include "ccm/ports/IImageStore.hpp"
#include "ccm/services/ImageService.hpp"

#include <map>
#include <string>
#include <vector>

using namespace ccm;

namespace {

// Trivial image store that records calls without touching disk.
class RecordingImageStore final : public IImageStore {
public:
    struct Call { Game game; std::filesystem::path src; std::string target; };
    std::vector<Call> copies;
    std::vector<std::pair<Game, std::string>> removes;
    std::string returnedExt = ".png";
    int failCopyAt = -1;
    int failRemoveAt = -1;

    Result<std::string> copyIn(Game game,
                               const std::filesystem::path& srcPath,
                               const std::string& targetName) override {
        copies.push_back({game, srcPath, targetName});
        if (failCopyAt >= 0 && static_cast<int>(copies.size()) == failCopyAt) {
            return Result<std::string>::err("copy failed at " + std::to_string(failCopyAt));
        }
        return Result<std::string>::ok(targetName + returnedExt);
    }

    Result<void> remove(Game game, const std::string& imageName) override {
        removes.emplace_back(game, imageName);
        if (failRemoveAt >= 0 && static_cast<int>(removes.size()) == failRemoveAt) {
            return Result<void>::err("remove failed at " + std::to_string(failRemoveAt));
        }
        return Result<void>::ok();
    }

    std::filesystem::path resolvePath(Game, const std::string& imageName) const override {
        return std::filesystem::path("/fake") / imageName;
    }
};

}  // namespace

TEST_SUITE("ImageService::nextImageIndex") {
    TEST_CASE("empty list returns 0") {
        CHECK(ImageService::nextImageIndex({}) == 0);
    }

    TEST_CASE("increments by one over the latest filename") {
        std::vector<std::string> imgs = {"set+name+0.png", "set+name+1.png"};
        CHECK(ImageService::nextImageIndex(imgs) == 2);
    }

    TEST_CASE("CCM1-style filenames reset back to 0") {
        std::vector<std::string> imgs = {"someCardIMG_FRONT.png"};
        CHECK(ImageService::nextImageIndex(imgs) == 0);
        std::vector<std::string> imgs2 = {"otherIMG_BACK.png"};
        CHECK(ImageService::nextImageIndex(imgs2) == 0);
    }

    TEST_CASE("index increments from two-digit legacy cap") {
        std::vector<std::string> imgs = {"set+name+99.png"};
        CHECK(ImageService::nextImageIndex(imgs) == 100);
    }

    TEST_CASE("three-digit filename index follows two-digit compatibility parser") {
        std::vector<std::string> imgs = {"set+name+255.png"};
        CHECK(ImageService::nextImageIndex(imgs) == 56);
    }
}

TEST_SUITE("ImageService::buildTargetName") {
    TEST_CASE("new entry omits id, uses sanitized set+name+idx") {
        const auto out = ImageService::buildTargetName(true, 99, "Limited: Alpha", "Lightning, Bolt", 3);
        CHECK(out == "Limited-Alpha+LightningBolt+3");
    }

    TEST_CASE("existing entry prepends the card id") {
        const auto out = ImageService::buildTargetName(false, 42, "Beta", "Black Lotus", 0);
        CHECK(out == "42+Beta+BlackLotus+0");
    }
}

TEST_SUITE("ImageService::addImage") {
    TEST_CASE("delegates to the store with the computed target name") {
        RecordingImageStore store;
        ImageService svc{store};

        std::vector<std::string> existing;
        auto res = svc.addImage(Game::Magic, "/tmp/source.png",
                                /*newEntry=*/true, /*cardId=*/0,
                                "Beta", "Black Lotus", existing);

        REQUIRE(res.isOk());
        CHECK(res.value() == "Beta+BlackLotus+0.png");
        REQUIRE(store.copies.size() == 1);
        CHECK(store.copies[0].game == Game::Magic);
        CHECK(store.copies[0].target == "Beta+BlackLotus+0");
    }

    TEST_CASE("propagates copyIn failures") {
        RecordingImageStore store;
        store.failCopyAt = 1;
        ImageService svc{store};

        std::vector<std::string> existing;
        const auto out = svc.addImage(Game::Magic, "/tmp/source.png",
                                      /*newEntry=*/true, /*cardId=*/0,
                                      "Beta", "Black Lotus", existing);
        REQUIRE(out.isErr());
        CHECK(out.error().find("copy failed at 1") != std::string::npos);
    }
}

TEST_SUITE("ImageService::removeImage and resolveImagePath") {
    TEST_CASE("removeImage delegates to store remove") {
        RecordingImageStore store;
        ImageService svc{store};

        const auto out = svc.removeImage(Game::Magic, "x.png");
        REQUIRE(out.isOk());
        REQUIRE(store.removes.size() == 1);
        CHECK(store.removes[0].first == Game::Magic);
        CHECK(store.removes[0].second == "x.png");
    }

    TEST_CASE("resolveImagePath delegates to store resolvePath") {
        RecordingImageStore store;
        ImageService svc{store};

        const auto p = svc.resolveImagePath(Game::Pokemon, "pikachu.jpg");
        CHECK(p == std::filesystem::path("/fake/pikachu.jpg"));
    }
}

TEST_SUITE("ImageService::normalizeNamesForPersistedCard") {
    TEST_CASE("renames non-prefixed images to include card id") {
        RecordingImageStore store;
        ImageService svc{store};

        const std::vector<std::string> images{
            "Beta+BlackLotus+0.png",
            "Beta+BlackLotus+1.jpg"
        };
        auto normalized = svc.normalizeNamesForPersistedCard(
            Game::Magic, 42, "Beta", "Black Lotus", images);

        REQUIRE(normalized.isOk());
        CHECK(normalized.value().size() == 2);
        CHECK(normalized.value()[0] == "42+Beta+BlackLotus+0.png");
        CHECK(normalized.value()[1] == "42+Beta+BlackLotus+1.jpg");

        REQUIRE(store.copies.size() == 2);
        CHECK(store.copies[0].src == std::filesystem::path("/fake/Beta+BlackLotus+0.png"));
        CHECK(store.copies[0].target == "42+Beta+BlackLotus+0");
        CHECK(store.copies[1].src == std::filesystem::path("/fake/Beta+BlackLotus+1.jpg"));
        CHECK(store.copies[1].target == "42+Beta+BlackLotus+1");

        REQUIRE(store.removes.size() == 2);
        CHECK(store.removes[0].second == "Beta+BlackLotus+0.png");
        CHECK(store.removes[1].second == "Beta+BlackLotus+1.jpg");
    }

    TEST_CASE("keeps already-prefixed names untouched") {
        RecordingImageStore store;
        ImageService svc{store};

        const std::vector<std::string> images{"42+Beta+BlackLotus+0.png"};
        auto normalized = svc.normalizeNamesForPersistedCard(
            Game::Magic, 42, "Beta", "Black Lotus", images);

        REQUIRE(normalized.isOk());
        CHECK(normalized.value() == images);
        CHECK(store.copies.empty());
        CHECK(store.removes.empty());
    }

    TEST_CASE("skips rename when computed output name equals input") {
        RecordingImageStore store;
        ImageService svc{store};

        const std::vector<std::string> images{"42+Beta+BlackLotus+0.png"};
        auto normalized = svc.normalizeNamesForPersistedCard(
            Game::Magic, 42, "Beta", "Black Lotus", images);

        REQUIRE(normalized.isOk());
        CHECK(normalized.value() == images);
        CHECK(store.copies.empty());
        CHECK(store.removes.empty());
    }

    TEST_CASE("copy failure rolls back already-created names and returns error") {
        RecordingImageStore store;
        store.failCopyAt = 2;
        ImageService svc{store};

        const std::vector<std::string> images{
            "Beta+BlackLotus+0.png",
            "Beta+BlackLotus+1.jpg"
        };
        const auto out = svc.normalizeNamesForPersistedCard(
            Game::Magic, 42, "Beta", "Black Lotus", images);

        REQUIRE(out.isErr());
        CHECK(out.error().find("copy failed at 2") != std::string::npos);
        // Second copy failed, so first created file should be rolled back.
        REQUIRE(store.removes.size() == 1);
        CHECK(store.removes[0].second == "42+Beta+BlackLotus+0.png");
    }

    TEST_CASE("remove failure after rename returns error") {
        RecordingImageStore store;
        store.failRemoveAt = 1;
        ImageService svc{store};

        const std::vector<std::string> images{
            "Beta+BlackLotus+0.png"
        };
        const auto out = svc.normalizeNamesForPersistedCard(
            Game::Magic, 42, "Beta", "Black Lotus", images);

        REQUIRE(out.isErr());
        CHECK(out.error().find("remove failed at 1") != std::string::npos);
    }
}
