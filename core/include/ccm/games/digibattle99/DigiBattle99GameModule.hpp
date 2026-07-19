#pragma once

// DigiBattle99GameModule: Digimon Digi-Battle (1999 English) via digimoncard.io.

#include "ccm/games/IGameModule.hpp"
#include "ccm/games/digibattle99/DigiBattle99CardPreviewSource.hpp"
#include "ccm/games/digibattle99/DigiBattle99SetSource.hpp"

namespace ccm {

class DigiBattle99GameModule final : public IGameModule {
public:
    explicit DigiBattle99GameModule(IHttpClient& http);

    [[nodiscard]] Game        id() const noexcept override { return Game::DigiBattle99; }
    [[nodiscard]] std::string dirName()     const override { return "digibattle99"; }
    [[nodiscard]] std::string displayName() const override { return "Digimon (Digi-Battle)"; }

    ISetSource&         setSource()          override { return setSource_; }
    ICardPreviewSource* cardPreviewSource() noexcept override { return &previewSource_; }

private:
    DigiBattle99SetSource         setSource_;
    DigiBattle99CardPreviewSource previewSource_;
};

}  // namespace ccm
