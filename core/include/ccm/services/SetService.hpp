#pragma once

// SetService: high-level operations for fetching and caching set data.
// Wraps an ISetRepository (cache) and dispatches to the correct ISetSource
// based on the active IGameModule.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/ISetRepository.hpp"
#include "ccm/util/Result.hpp"

#include <unordered_map>
#include <vector>

namespace ccm {

class SetService {
public:
    explicit SetService(ISetRepository& repo);

    // Register a game module so this service can route fetch requests.
    // Pointer must remain valid for the lifetime of the SetService.
    void registerModule(IGameModule* module);

    // Force a fresh fetch from the API for `game`, persist it via the
    // repository, and return the new list.
    Result<std::vector<Set>> updateSets(Game game);

    // Cached read; returns an error if no local data exists yet.
    Result<std::vector<Set>> getSets(Game game);

private:
    ISetRepository& repo_;
    std::unordered_map<Game, IGameModule*> modules_;
};

}  // namespace ccm
