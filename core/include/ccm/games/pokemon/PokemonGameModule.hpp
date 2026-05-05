#pragma once

// PokemonGameModule: stub IGameModule for the Pokemon TCG.
// Wiring is in place so the UI can offer Pokemon menu items, but the set
// source returns a "not implemented" result until the API parser is filled in.

#include "ccm/games/IGameModule.hpp"
#include "ccm/games/pokemon/PokemonSetSource.hpp"

namespace ccm {

class PokemonGameModule final : public IGameModule {
public:
    explicit PokemonGameModule(IHttpClient& http);

    [[nodiscard]] Game        id() const noexcept override { return Game::Pokemon; }
    [[nodiscard]] std::string dirName()     const override { return "pokemon"; }
    [[nodiscard]] std::string displayName() const override { return "Pokemon"; }

    ISetSource& setSource() override { return setSource_; }

private:
    PokemonSetSource setSource_;
};

}  // namespace ccm
