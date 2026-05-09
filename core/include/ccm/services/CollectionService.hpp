#pragma once

// CollectionService<TCard>: high-level CRUD over the per-game collection.
// Header-only template that operates on the ICollectionRepository<T> port and
// delegates image cleanup to an IImageStore on remove.
//
// This is the C++ equivalent of the Rust generic helpers in
// `templates/card_service_templates.rs` (add_entry_to_collection,
// update_entry_in_collection, get_entry_by_id, delete_entry_by_id, ...).
//
// Each TCard must expose `std::uint32_t id` and `std::vector<std::string> images`.

#include "ccm/domain/Enums.hpp"
#include "ccm/ports/ICollectionRepository.hpp"
#include "ccm/ports/IImageStore.hpp"
#include "ccm/util/Result.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ccm {

template <typename TCard>
class CollectionService {
public:
    using Map = std::map<std::uint32_t, TCard>;

    CollectionService(ICollectionRepository<TCard>& repo, IImageStore& imageStore)
        : repo_(repo), imageStore_(imageStore) {}

    Result<std::vector<TCard>> list(Game game) {
        auto loaded = repo_.load(game);
        if (!loaded) return Result<std::vector<TCard>>::err(loaded.error());
        Map map = std::move(loaded).value();
        std::vector<TCard> out;
        out.reserve(map.size());
        for (auto& [id, card] : map) {
            (void)id;
            out.push_back(std::move(card));
        }
        return Result<std::vector<TCard>>::ok(std::move(out));
    }

    [[nodiscard]] static std::uint32_t nextId(const Map& map) noexcept {
        if (map.empty()) return 0;
        return map.rbegin()->first + 1;
    }

    // Add a card. The `id` field on the input is overwritten with the next
    // free id (matching the Rust HashMap-keyed behavior).
    Result<std::uint32_t> add(Game game, TCard card) {
        auto loaded = repo_.load(game);
        if (!loaded) return Result<std::uint32_t>::err(loaded.error());
        Map map = std::move(loaded).value();
        const std::uint32_t newId = nextId(map);
        card.id = newId;
        map.emplace(newId, std::move(card));
        auto saved = repo_.save(game, map);
        if (!saved) return Result<std::uint32_t>::err(saved.error());
        return Result<std::uint32_t>::ok(newId);
    }

    // Update an existing entry, identified by `card.id`. If the id is not
    // present, an error is returned.
    Result<void> update(Game game, TCard card) {
        auto loaded = repo_.load(game);
        if (!loaded) return Result<void>::err(loaded.error());
        Map map = std::move(loaded).value();
        auto it = map.find(card.id);
        if (it == map.end()) {
            return Result<void>::err("Card with id " + std::to_string(card.id) +
                                     " not found in collection.");
        }
        it->second = std::move(card);
        return repo_.save(game, map);
    }

    // Remove the card with the given id. Also deletes any associated images
    // via the IImageStore (best-effort - image removal failures are logged in
    // the error string but the card itself is still purged from the JSON).
    Result<void> remove(Game game, std::uint32_t id) {
        auto loaded = repo_.load(game);
        if (!loaded) return Result<void>::err(loaded.error());
        Map map = std::move(loaded).value();
        auto it = map.find(id);
        if (it == map.end()) {
            return Result<void>::err("Card with id " + std::to_string(id) + " not found.");
        }
        std::string imgErr;
        for (const auto& image : it->second.images) {
            auto rm = imageStore_.remove(game, image);
            if (!rm) {
                if (!imgErr.empty()) imgErr += "; ";
                imgErr += rm.error();
            }
        }
        map.erase(it);
        auto saved = repo_.save(game, map);
        if (!saved) return saved;
        if (!imgErr.empty()) {
            return Result<void>::err("Card removed but image cleanup had issues: " + imgErr);
        }
        return Result<void>::ok();
    }

    // Look up a card by id without mutating storage.
    Result<std::optional<TCard>> findById(Game game, std::uint32_t id) {
        auto loaded = repo_.load(game);
        if (!loaded) return Result<std::optional<TCard>>::err(loaded.error());
        const auto& m = loaded.value();
        auto it = m.find(id);
        if (it == m.end()) return Result<std::optional<TCard>>::ok(std::nullopt);
        return Result<std::optional<TCard>>::ok(it->second);
    }

private:
    ICollectionRepository<TCard>& repo_;
    IImageStore&                  imageStore_;
};

}  // namespace ccm
