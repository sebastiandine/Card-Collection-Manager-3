#pragma once

// PokemonSetSource: ISetSource stub for the Pokemon TCG.
//
// The shape mirrors MagicSetSource so finishing the implementation is a
// matter of pointing it at https://api.pokemontcg.io/v2/sets and parsing the
// response. Until then, fetchAll() returns a clear "not implemented" error
// for the UI to display.

#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

namespace ccm {

class PokemonSetSource final : public ISetSource {
public:
    static constexpr const char* kEndpoint = "https://api.pokemontcg.io/v2/sets";

    explicit PokemonSetSource(IHttpClient& http);

    Result<std::vector<Set>> fetchAll() override;

private:
    IHttpClient& http_;
};

}  // namespace ccm
