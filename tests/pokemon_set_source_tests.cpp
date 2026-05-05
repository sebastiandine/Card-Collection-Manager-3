#include <doctest/doctest.h>

#include "ccm/games/pokemon/PokemonSetSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

using namespace ccm;

namespace {
class NoopHttpClient final : public IHttpClient {
public:
    Result<std::string> get(std::string_view) override {
        return Result<std::string>::err("should not be called");
    }
};
}  // namespace

TEST_SUITE("PokemonSetSource (stub)") {
    TEST_CASE("fetchAll surfaces a 'not implemented' message without hitting the network") {
        NoopHttpClient http;
        PokemonSetSource src{http};
        const auto out = src.fetchAll();
        REQUIRE(out.isErr());
        CHECK(out.error().find("not implemented") != std::string::npos);
    }
}
