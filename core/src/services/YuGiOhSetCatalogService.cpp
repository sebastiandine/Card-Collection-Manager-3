#include "ccm/services/YuGiOhSetCatalogService.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace ccm {

namespace fs = std::filesystem;

YuGiOhSetCatalogService::YuGiOhSetCatalogService(IFileSystem&   fs,
                                                 ConfigService& config,
                                                 DirNameFn      dirName)
    : fs_(fs), config_(config), dirName_(std::move(dirName)) {}

fs::path YuGiOhSetCatalogService::catalogPath() const {
    return fs::path(config_.current().dataStorage) / dirName_(Game::YuGiOh) /
           "set-catalog.json";
}

bool YuGiOhSetCatalogService::exists() const {
    return fs_.exists(catalogPath());
}

Result<YuGiOhSetCatalog> YuGiOhSetCatalogService::load() const {
    const auto p = catalogPath();
    if (!fs_.exists(p)) {
        return Result<YuGiOhSetCatalog>::err("Yu-Gi-Oh! set catalog not yet downloaded.");
    }
    auto text = fs_.readText(p);
    if (!text) return Result<YuGiOhSetCatalog>::err(text.error());
    try {
        const auto j = nlohmann::json::parse(text.value());
        return Result<YuGiOhSetCatalog>::ok(j.get<YuGiOhSetCatalog>());
    } catch (const std::exception& e) {
        return Result<YuGiOhSetCatalog>::err(
            std::string("set-catalog.json parse error: ") + e.what());
    }
}

Result<void> YuGiOhSetCatalogService::save(const YuGiOhSetCatalog& catalog) {
    const auto p = catalogPath();
    auto dir = fs_.ensureDirectory(p.parent_path());
    if (!dir) return dir;
    const nlohmann::json j = catalog;
    return fs_.writeText(p, j.dump(2));
}

}  // namespace ccm
