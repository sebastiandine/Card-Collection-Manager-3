#pragma once

#include "ccm/games/IGameModule.hpp"
#include "ccm/games/yugioh/YuGiOhCardPreviewSource.hpp"
#include "ccm/games/yugioh/YuGiOhSetSource.hpp"
#include "ccm/services/YuGiOhSetCatalogService.hpp"

namespace ccm {

class YuGiOhGameModule final : public IGameModule {
public:
    explicit YuGiOhGameModule(IHttpClient& http);

    [[nodiscard]] Game        id() const noexcept override { return Game::YuGiOh; }
    [[nodiscard]] std::string dirName()     const override { return "yugioh"; }
    [[nodiscard]] std::string displayName() const override { return "Yu-Gi-Oh!"; }

    ISetSource&         setSource()          override { return setSource_; }
    ICardPreviewSource* cardPreviewSource() noexcept override { return &previewSource_; }

    // Wire offline set catalog for set+setNo → name reverse auto-detect.
    void setCatalogService(YuGiOhSetCatalogService* catalogStore) noexcept {
        previewSource_.setCatalogService(catalogStore);
    }

    [[nodiscard]] YuGiOhCardPreviewSource& previewSource() noexcept {
        return previewSource_;
    }

private:
    YuGiOhSetSource          setSource_;
    YuGiOhCardPreviewSource  previewSource_;
};

}  // namespace ccm
