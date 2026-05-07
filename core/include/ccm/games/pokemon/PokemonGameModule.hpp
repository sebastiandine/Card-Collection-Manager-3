#pragma once

// PokemonGameModule: IGameModule for the Pokemon TCG. Owns its set source
// and card preview source, both backed by api.pokemontcg.io/v2.

#include "ccm/games/IGameModule.hpp"
#include "ccm/games/pokemon/PokemonCardPreviewSource.hpp"
#include "ccm/games/pokemon/PokemonSetSource.hpp"

namespace ccm {

class PokemonGameModule final : public IGameModule {
public:
    explicit PokemonGameModule(IHttpClient& http);

    [[nodiscard]] Game        id() const noexcept override { return Game::Pokemon; }
    [[nodiscard]] std::string dirName()     const override { return "pokemon"; }
    [[nodiscard]] std::string displayName() const override { return "Pokemon"; }

    ISetSource&         setSource()          override { return setSource_; }
    ICardPreviewSource* cardPreviewSource() noexcept override { return &previewSource_; }

private:
    PokemonSetSource          setSource_;
    PokemonCardPreviewSource  previewSource_;
};

}  // namespace ccm
