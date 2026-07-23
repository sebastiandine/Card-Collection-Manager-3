#include "ccm/services/PokemonSetCatalogService.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace ccm {

namespace fs = std::filesystem;

PokemonSetCatalogService::PokemonSetCatalogService(IFileSystem&   fs,
                                                   ConfigService& config,
                                                   DirNameFn      dirName)
    : fs_(fs), config_(config), dirName_(std::move(dirName)) {}

fs::path PokemonSetCatalogService::catalogPath(PokemonRegion region) const {
    const char* file = region == PokemonRegion::Asia ? "set-catalog-asia.json"
                                                     : "set-catalog-west.json";
    return fs::path(config_.current().dataStorage) / dirName_(Game::Pokemon) / file;
}

bool PokemonSetCatalogService::exists(PokemonRegion region) const {
    return fs_.exists(catalogPath(region));
}

Result<PokemonSetCatalog> PokemonSetCatalogService::load(PokemonRegion region) const {
    const auto p = catalogPath(region);
    if (!fs_.exists(p)) {
        return Result<PokemonSetCatalog>::err(
            region == PokemonRegion::Asia
                ? "Asia Pokemon set catalog not yet downloaded."
                : "West Pokemon set catalog not yet downloaded.");
    }
    auto text = fs_.readText(p);
    if (!text) return Result<PokemonSetCatalog>::err(text.error());
    try {
        const auto j = nlohmann::json::parse(text.value());
        return Result<PokemonSetCatalog>::ok(j.get<PokemonSetCatalog>());
    } catch (const std::exception& e) {
        return Result<PokemonSetCatalog>::err(
            std::string("set-catalog.json parse error: ") + e.what());
    }
}

Result<void> PokemonSetCatalogService::save(PokemonRegion             region,
                                            const PokemonSetCatalog&  catalog) {
    const auto p = catalogPath(region);
    auto dir = fs_.ensureDirectory(p.parent_path());
    if (!dir) return dir;
    const nlohmann::json j = catalog;
    return fs_.writeText(p, j.dump(2));
}

}  // namespace ccm
