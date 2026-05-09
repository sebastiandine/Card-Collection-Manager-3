#include "ccm/infra/JsonSetRepository.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace ccm {

namespace fs = std::filesystem;

JsonSetRepository::JsonSetRepository(IFileSystem& fs, ConfigService& config, DirNameFn dirName)
    : fs_(fs), config_(config), dirName_(std::move(dirName)) {}

fs::path JsonSetRepository::setsPath(Game game) const {
    return fs::path(config_.current().dataStorage) / dirName_(game) / "sets.json";
}

Result<std::vector<Set>> JsonSetRepository::load(Game game) {
    const auto p = setsPath(game);
    if (!fs_.exists(p)) {
        return Result<std::vector<Set>>::err("Set list not yet downloaded for this game.");
    }
    auto text = fs_.readText(p);
    if (!text) return Result<std::vector<Set>>::err(text.error());
    try {
        auto j = nlohmann::json::parse(text.value());
        return Result<std::vector<Set>>::ok(j.get<std::vector<Set>>());
    } catch (const std::exception& e) {
        return Result<std::vector<Set>>::err(std::string("sets.json parse error: ") + e.what());
    }
}

Result<void> JsonSetRepository::save(Game game, const std::vector<Set>& sets) {
    const auto p = setsPath(game);
    auto dir = fs_.ensureDirectory(p.parent_path());
    if (!dir) return dir;
    const nlohmann::json j = sets;
    return fs_.writeText(p, j.dump(2));
}

}  // namespace ccm
