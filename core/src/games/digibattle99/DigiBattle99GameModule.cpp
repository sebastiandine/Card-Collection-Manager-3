#include "ccm/games/digibattle99/DigiBattle99GameModule.hpp"

namespace ccm {

DigiBattle99GameModule::DigiBattle99GameModule(IHttpClient& http)
    : setSource_(http), previewSource_(http) {}

}  // namespace ccm
