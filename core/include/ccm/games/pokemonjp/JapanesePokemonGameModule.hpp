#pragma once

// JapanesePokemonGameModule: Japanese Pokémon TCG via TCGdex ja + EN catalog.

#include "ccm/games/IGameModule.hpp"
#include "ccm/games/pokemonjp/JapanesePokemonCardPreviewSource.hpp"
#include "ccm/games/pokemonjp/JapanesePokemonEnCatalog.hpp"
#include "ccm/games/pokemonjp/JapanesePokemonSetSource.hpp"

namespace ccm {

class JapanesePokemonGameModule final : public IGameModule {
public:
    explicit JapanesePokemonGameModule(IHttpClient& http,
                                       JapanesePokemonEnCatalog catalog = {});

    [[nodiscard]] Game        id() const noexcept override { return Game::JapanesePokemon; }
    [[nodiscard]] std::string dirName()     const override { return "pokemonjp"; }
    [[nodiscard]] std::string displayName() const override { return "Pokemon (Japan)"; }

    ISetSource&         setSource()          override { return setSource_; }
    ICardPreviewSource* cardPreviewSource() noexcept override { return &previewSource_; }

    [[nodiscard]] const JapanesePokemonEnCatalog& catalog() const noexcept {
        return catalog_;
    }

private:
    JapanesePokemonEnCatalog          catalog_;
    JapanesePokemonSetSource          setSource_;
    JapanesePokemonCardPreviewSource  previewSource_;
};

}  // namespace ccm
