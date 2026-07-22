#include "ccm/games/pokemonjp/JapanesePokemonGameModule.hpp"

namespace ccm {

JapanesePokemonGameModule::JapanesePokemonGameModule(IHttpClient& http,
                                                     JapanesePokemonEnCatalog catalog)
    : catalog_(std::move(catalog)),
      setSource_(http, catalog_),
      previewSource_(http, catalog_) {}

}  // namespace ccm
