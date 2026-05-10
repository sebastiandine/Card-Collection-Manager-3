#include <doctest/doctest.h>

#include "ccm/games/magic/MagicGameModule.hpp"
#include "ccm/games/pokemon/PokemonGameModule.hpp"
#include "ccm/games/yugioh/YuGiOhGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

using namespace ccm;

namespace {

class NoopHttpClient final : public IHttpClient {
public:
    Result<std::string> get(std::string_view) override {
        return Result<std::string>::ok("{}");
    }
};

}  // namespace

TEST_SUITE("game modules expose stable identity and wiring") {
    TEST_CASE("Magic module reports canonical metadata") {
        NoopHttpClient http;
        MagicGameModule module(http);

        CHECK(module.id() == Game::Magic);
        CHECK(module.dirName() == "magic");
        CHECK(module.displayName() == "Magic");
        CHECK(module.cardPreviewSource() != nullptr);
    }

    TEST_CASE("Pokemon module reports canonical metadata") {
        NoopHttpClient http;
        PokemonGameModule module(http);

        CHECK(module.id() == Game::Pokemon);
        CHECK(module.dirName() == "pokemon");
        CHECK(module.displayName() == "Pokemon");
        CHECK(module.cardPreviewSource() != nullptr);
    }

    TEST_CASE("YuGiOh module reports canonical metadata") {
        NoopHttpClient http;
        YuGiOhGameModule module(http);

        CHECK(module.id() == Game::YuGiOh);
        CHECK(module.dirName() == "yugioh");
        CHECK(module.displayName() == "Yu-Gi-Oh!");
        CHECK(module.cardPreviewSource() != nullptr);
    }
}
