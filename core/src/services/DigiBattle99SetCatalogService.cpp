#include "ccm/services/DigiBattle99SetCatalogService.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace ccm {

namespace fs = std::filesystem;

DigiBattle99SetCatalogService::DigiBattle99SetCatalogService(IFileSystem&   fs,
                                                             ConfigService& config,
                                                             DirNameFn      dirName)
    : fs_(fs), config_(config), dirName_(std::move(dirName)) {}

fs::path DigiBattle99SetCatalogService::catalogPath() const {
    return fs::path(config_.current().dataStorage) / dirName_(Game::DigiBattle99) /
           "set-catalog.json";
}

bool DigiBattle99SetCatalogService::exists() const {
    return fs_.exists(catalogPath());
}

Result<DigiBattle99SetCatalog> DigiBattle99SetCatalogService::load() const {
    const auto p = catalogPath();
    if (!fs_.exists(p)) {
        return Result<DigiBattle99SetCatalog>::err(
            "Digimon Digi-Battle set catalog not yet downloaded.");
    }
    auto text = fs_.readText(p);
    if (!text) return Result<DigiBattle99SetCatalog>::err(text.error());
    try {
        const auto j = nlohmann::json::parse(text.value());
        return Result<DigiBattle99SetCatalog>::ok(j.get<DigiBattle99SetCatalog>());
    } catch (const std::exception& e) {
        return Result<DigiBattle99SetCatalog>::err(
            std::string("set-catalog.json parse error: ") + e.what());
    }
}

Result<void> DigiBattle99SetCatalogService::save(const DigiBattle99SetCatalog& catalog) {
    const auto p = catalogPath();
    auto dir = fs_.ensureDirectory(p.parent_path());
    if (!dir) return dir;
    const nlohmann::json j = catalog;
    return fs_.writeText(p, j.dump(2));
}

}  // namespace ccm
