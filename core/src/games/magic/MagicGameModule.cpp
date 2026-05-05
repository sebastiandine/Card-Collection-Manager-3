#include "ccm/games/magic/MagicGameModule.hpp"

namespace ccm {

MagicGameModule::MagicGameModule(IHttpClient& http) : setSource_(http) {}

}  // namespace ccm
