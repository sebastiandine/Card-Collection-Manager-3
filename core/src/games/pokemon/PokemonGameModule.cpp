#include "ccm/games/pokemon/PokemonGameModule.hpp"

namespace ccm {

PokemonGameModule::PokemonGameModule(IHttpClient& http)
    : setSource_(http), previewSource_(http) {}

}  // namespace ccm
