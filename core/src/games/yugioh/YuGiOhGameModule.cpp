#include "ccm/games/yugioh/YuGiOhGameModule.hpp"

namespace ccm {

YuGiOhGameModule::YuGiOhGameModule(IHttpClient& http)
    : setSource_(http), previewSource_(http) {}

}  // namespace ccm
