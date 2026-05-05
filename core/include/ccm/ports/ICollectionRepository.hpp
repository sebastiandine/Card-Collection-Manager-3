#pragma once

// ICollectionRepository<T> - persistence port for a per-game card collection,
// keyed by uint32_t id. Mirrors the HashMap<u32, T> in the original Rust code.

#include "ccm/domain/Enums.hpp"
#include "ccm/util/Result.hpp"

#include <cstdint>
#include <map>

namespace ccm {

template <typename TCard>
class ICollectionRepository {
public:
    using Map = std::map<std::uint32_t, TCard>;

    virtual ~ICollectionRepository() = default;

    virtual Result<Map>  load(Game game) = 0;
    virtual Result<void> save(Game game, const Map& collection) = 0;
};

}  // namespace ccm
