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

    Result<std::string> copyIn(Game game,
                               const std::filesystem::path& srcPath,
                               const std::string& targetName) override {
        copies.push_back({game, srcPath, targetName});
        return Result<std::string>::ok(targetName + returnedExt);
    }

    Result<void> remove(Game game, const std::string& imageName) override {
        removes.emplace_back(game, imageName);
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
}
