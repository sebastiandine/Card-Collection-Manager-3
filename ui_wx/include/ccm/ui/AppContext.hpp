#pragma once

// AppContext: the only thing that crosses the UI boundary. Holds references
// to the shared core services and to the per-game `IGameView` instances. The
// wxWidgets layer never sees a concrete adapter type or a typed
// `CollectionService<TCard>` - swap in a Qt/imgui frontend by reimplementing
// the consumers of this struct only.

#include "ccm/games/IGameModule.hpp"
#include "ccm/services/CardPreviewService.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/services/ImageService.hpp"
#include "ccm/services/SetService.hpp"

#include <vector>

namespace ccm::ui {

class IGameView;

struct AppContext {
    ConfigService&                       config;
    SetService&                          sets;
    ImageService&                        images;
    CardPreviewService&                  cardPreview;
    IGameModule&                         magicModule;
    IGameModule&                         pokemonModule;
    // Active per-game UI bundles. The order is the order shown in the
    // Game menu; the composition root constructs them and hands raw
    // pointers in. `MainFrame` does not own these — `app/main.cpp` does.
    std::vector<IGameView*>              gameViews;
};

}  // namespace ccm::ui
