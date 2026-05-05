#include "ccm/games/pokemon/PokemonGameModule.hpp"

namespace ccm {

PokemonGameModule::PokemonGameModule(IHttpClient& http) : setSource_(http) {}

}  // namespace ccm
