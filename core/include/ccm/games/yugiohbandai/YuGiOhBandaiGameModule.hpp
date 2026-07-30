#pragma once

// YuGiOhBandaiGameModule: Bandai Carddass via Yugipedia.

#include "ccm/games/IGameModule.hpp"
#include "ccm/games/yugiohbandai/YuGiOhBandaiCardPreviewSource.hpp"
#include "ccm/games/yugiohbandai/YuGiOhBandaiSetSource.hpp"

namespace ccm {

class YuGiOhBandaiGameModule final : public IGameModule {
public:
    explicit YuGiOhBandaiGameModule(IHttpClient& http);

    [[nodiscard]] Game        id() const noexcept override { return Game::YuGiOhBandai; }
    [[nodiscard]] std::string dirName()     const override { return "yugiohbandai"; }
    [[nodiscard]] std::string displayName() const override { return "Yu-Gi-Oh! (Bandai)"; }

    ISetSource&         setSource()          override { return setSource_; }
    ICardPreviewSource* cardPreviewSource() noexcept override { return &previewSource_; }

    YuGiOhBandaiSetSource& bandaiSetSource() noexcept { return setSource_; }

private:
    YuGiOhBandaiSetSource         setSource_;
    YuGiOhBandaiCardPreviewSource previewSource_;
};

}  // namespace ccm
