#pragma once

// IGameModule + ISetSource: the seam that lets the UI handle Magic and Pokemon
// uniformly. New game support is "implement these two interfaces and register
// them in the composition root" - see the Rust `templates/` module for the
// pattern this is modeled after.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/util/Result.hpp"

#include <string>
#include <vector>

namespace ccm {

class ISetSource {
public:
    virtual ~ISetSource() = default;

    // Fetch the canonical set list from the game's external API.
    // Implementations return a vector that has already been filtered
    // (e.g. no digital-only sets) and sorted by release date ascending.
    virtual Result<std::vector<Set>> fetchAll() = 0;
};

class IGameModule {
public:
    virtual ~IGameModule() = default;

    [[nodiscard]] virtual Game        id() const noexcept = 0;
    // Subdirectory name used inside the data storage root. Matches the Rust
    // string literals "magic" / "pokemon".
    [[nodiscard]] virtual std::string dirName() const = 0;
    [[nodiscard]] virtual std::string displayName() const = 0;

    virtual ISetSource& setSource() = 0;
};

}  // namespace ccm
