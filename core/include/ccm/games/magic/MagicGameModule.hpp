#pragma once

// MagicGameModule: IGameModule for Magic the Gathering. Owns its set source
// and reports the canonical "magic" subdirectory name used on disk.

#include "ccm/games/IGameModule.hpp"
#include "ccm/games/magic/MagicSetSource.hpp"

namespace ccm {

class MagicGameModule final : public IGameModule {
public:
    explicit MagicGameModule(IHttpClient& http);

    [[nodiscard]] Game        id() const noexcept override { return Game::Magic; }
    [[nodiscard]] std::string dirName()     const override { return "magic"; }
    [[nodiscard]] std::string displayName() const override { return "Magic"; }

    ISetSource& setSource() override { return setSource_; }

private:
    MagicSetSource setSource_;
};

}  // namespace ccm
