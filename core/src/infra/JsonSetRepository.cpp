#include "ccm/infra/JsonSetRepository.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace ccm {

namespace fs = std::filesystem;

JsonSetRepository::JsonSetRepository(IFileSystem& fs, ConfigService& config, DirNameFn dirName)
    : fs_(fs), config_(config), dirName_(std::move(dirName)) {}

fs::path JsonSetRepository::setsPath(Game game) const {
    const fs::path root = fs::path(config_.current().dataStorage) / dirName_(game);
    switch (game) {
        case Game::Pokemon:         return root / "sets-west.json";
        case Game::JapanesePokemon: return root / "sets-asia.json";
        default:                    return root / "sets.json";
    }
}

fs::path JsonSetRepository::legacySetsPath(Game game) const {
    const fs::path dataRoot(config_.current().dataStorage);
    switch (game) {
        case Game::Pokemon:
            // Pre-flatten: pokemon/sets.json
            return dataRoot / "pokemon" / "sets.json";
        case Game::JapanesePokemon:
            // Pre-flatten: pokemonjp/sets.json
            return dataRoot / "pokemonjp" / "sets.json";
        default:
            return {};
    }
}

Result<std::vector<Set>> JsonSetRepository::parseSetsText(const std::string& text) const {
    try {
        auto j = nlohmann::json::parse(text);
        return Result<std::vector<Set>>::ok(j.get<std::vector<Set>>());
    } catch (const std::exception& e) {
        return Result<std::vector<Set>>::err(std::string("sets.json parse error: ") + e.what());
    }
}

Result<std::vector<Set>> JsonSetRepository::load(Game game) {
    const auto p = setsPath(game);
    if (fs_.exists(p)) {
        auto text = fs_.readText(p);
        if (!text) return Result<std::vector<Set>>::err(text.error());
        return parseSetsText(text.value());
    }

    const auto legacy = legacySetsPath(game);
    if (!legacy.empty() && fs_.exists(legacy)) {
        auto text = fs_.readText(legacy);
        if (!text) return Result<std::vector<Set>>::err(text.error());
        auto parsed = parseSetsText(text.value());
        if (!parsed) return parsed;
        // Best-effort promote to the new path; UI still gets the sets if write fails.
        (void)save(game, parsed.value());
        return parsed;
    }

    return Result<std::vector<Set>>::err("Set list not yet downloaded for this game.");
}

Result<void> JsonSetRepository::save(Game game, const std::vector<Set>& sets) {
    const auto p = setsPath(game);
    auto dir = fs_.ensureDirectory(p.parent_path());
    if (!dir) return dir;
    const nlohmann::json j = sets;
    return fs_.writeText(p, j.dump(2));
}

}  // namespace ccm
