#include "ccm/games/yugiohbandai/YuGiOhBandaiGameModule.hpp"

namespace ccm {

YuGiOhBandaiGameModule::YuGiOhBandaiGameModule(IHttpClient& http)
    : setSource_(http), previewSource_(http) {}

}  // namespace ccm
