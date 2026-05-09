#pragma once

// ISetRepository - persistence port for the cached `sets.json` of a game.
// Stored as a flat list to mirror the original Rust file layout.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/util/Result.hpp"

#include <vector>

namespace ccm {

class ISetRepository {
public:
    virtual ~ISetRepository() = default;

    virtual Result<std::vector<Set>> load(Game game) = 0;
    virtual Result<void>             save(Game game, const std::vector<Set>& sets) = 0;
};

}  // namespace ccm
