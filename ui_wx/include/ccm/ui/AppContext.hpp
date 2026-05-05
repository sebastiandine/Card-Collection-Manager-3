#pragma once

// AppContext: the only thing that crosses the UI boundary. Holds references
// to the core services and game modules. The wxWidgets layer never sees a
// concrete adapter type - swap in a Qt/imgui frontend by reimplementing the
// consumers of this struct only.

#include "ccm/domain/MagicCard.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/services/CollectionService.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/services/SetService.hpp"

namespace ccm::ui {

struct AppContext {
    ConfigService&                       config;
    CollectionService<MagicCard>&        magicCollection;
    SetService&                          sets;
    ImageService&                        images;
    CardPreviewService&                  cardPreview;
    IGameModule&                         magicModule;
    IGameModule&                         pokemonModule;
};

}  // namespace ccm::ui
