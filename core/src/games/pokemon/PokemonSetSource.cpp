#include "ccm/games/pokemon/PokemonSetSource.hpp"

namespace ccm {

PokemonSetSource::PokemonSetSource(IHttpClient& http) : http_(http) {}

Result<std::vector<Set>> PokemonSetSource::fetchAll() {
    return Result<std::vector<Set>>::err(
        "Pokemon TCG support is not implemented yet. "
        "See PokemonSetSource for a wiring point.");
}

}  // namespace ccm
