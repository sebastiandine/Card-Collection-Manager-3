#pragma once

// PokemonSetSource: ISetSource implementation for the Pokemon TCG.
// Calls the Pokemon TCG API at https://api.pokemontcg.io/v2/sets, maps the
// response into our `Set` domain type, and sorts by release date ascending.
// The Pokemon TCG API already returns `releaseDate` in `YYYY/MM/DD` format,
// so no rewriting is needed (unlike Scryfall's `released_at`).
// Behavior matches `pokemon/set_services.rs::update_sets`.

#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

namespace ccm {

class PokemonSetSource final : public ISetSource {
public:
    static constexpr const char* kEndpoint = "https://api.pokemontcg.io/v2/sets";

    explicit PokemonSetSource(IHttpClient& http);

    Result<std::vector<Set>> fetchAll() override;

    // Pure parser exposed for unit testing without a network round-trip.
    static Result<std::vector<Set>> parseResponse(const std::string& body);

private:
    IHttpClient& http_;
};

}  // namespace ccm
