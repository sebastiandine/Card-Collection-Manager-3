#pragma once

// JsonCollectionRepository<T>: persists std::map<id, TCard> as a JSON object
// keyed by stringified id. Layout matches the established collection file:
//
//   <dataStorage>/<gameDir>/collection.json   ->   { "0": {...}, "1": {...} }
//
// Header-only because it's a template over the card type.

#include "ccm/domain/Configuration.hpp"
#include "ccm/domain/Enums.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/ICollectionRepository.hpp"
#include "ccm/ports/IFileSystem.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/util/Result.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <functional>
#include <string>

namespace ccm {

template <typename TCard>
class JsonCollectionRepository final : public ICollectionRepository<TCard> {
public:
    using Map = typename ICollectionRepository<TCard>::Map;
    // The repository needs a way to translate a Game enum into the per-game
    // subdirectory name ("magic" / "pokemon"); a small lookup function keeps
    // this layer free of concrete game module types.
    using DirNameFn = std::function<std::string(Game)>;

    JsonCollectionRepository(IFileSystem& fs, ConfigService& config, DirNameFn dirName)
        : fs_(fs), config_(config), dirName_(std::move(dirName)) {}

    Result<Map> load(Game game) override {
        const auto p = collectionPath(game);
        if (!fs_.exists(p)) {
            // Mirror the Rust "create on first read" semantics so the UI
            // never sees a missing-file error on a fresh install.
            Map empty;
            auto saved = save(game, empty);
            if (!saved) return Result<Map>::err(saved.error());
            return Result<Map>::ok(std::move(empty));
        }
        auto text = fs_.readText(p);
        if (!text) return Result<Map>::err(text.error());
        try {
            auto j = nlohmann::json::parse(text.value());
            Map out;
            for (auto it = j.begin(); it != j.end(); ++it) {
                std::uint32_t key = static_cast<std::uint32_t>(std::stoul(it.key()));
                out.emplace(key, it.value().template get<TCard>());
            }
            return Result<Map>::ok(std::move(out));
        } catch (const std::exception& e) {
            return Result<Map>::err(std::string("JSON parse error: ") + e.what());
        }
    }

    Result<void> save(Game game, const Map& collection) override {
        nlohmann::json j = nlohmann::json::object();
        for (const auto& [id, card] : collection) {
            j[std::to_string(id)] = card;
        }
        const auto p = collectionPath(game);
        auto dirRes = fs_.ensureDirectory(p.parent_path());
        if (!dirRes) return dirRes;
        return fs_.writeText(p, j.dump(2));
    }

private:
    std::filesystem::path collectionPath(Game game) const {
        return std::filesystem::path(config_.current().dataStorage) /
               dirName_(game) / "collection.json";
    }

    IFileSystem&    fs_;
    ConfigService&  config_;
    DirNameFn       dirName_;
};

}  // namespace ccm
