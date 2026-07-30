#include "ccm/services/YuGiOhBandaiSetCatalogService.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace ccm {

namespace fs = std::filesystem;

YuGiOhBandaiSetCatalogService::YuGiOhBandaiSetCatalogService(IFileSystem&   fs,
                                                             ConfigService& config,
                                                             DirNameFn      dirName)
    : fs_(fs), config_(config), dirName_(std::move(dirName)) {}

fs::path YuGiOhBandaiSetCatalogService::catalogPath() const {
    return fs::path(config_.current().dataStorage) / dirName_(Game::YuGiOhBandai) /
           "set-catalog.json";
}

bool YuGiOhBandaiSetCatalogService::exists() const {
    return fs_.exists(catalogPath());
}

Result<YuGiOhBandaiSetCatalog> YuGiOhBandaiSetCatalogService::load() const {
    const auto p = catalogPath();
    if (!fs_.exists(p)) {
        return Result<YuGiOhBandaiSetCatalog>::err(
            "Yu-Gi-Oh! (Bandai) set catalog not yet downloaded.");
    }
    auto text = fs_.readText(p);
    if (!text) return Result<YuGiOhBandaiSetCatalog>::err(text.error());
    try {
        const auto j = nlohmann::json::parse(text.value());
        return Result<YuGiOhBandaiSetCatalog>::ok(j.get<YuGiOhBandaiSetCatalog>());
    } catch (const std::exception& e) {
        return Result<YuGiOhBandaiSetCatalog>::err(
            std::string("set-catalog.json parse error: ") + e.what());
    }
}

Result<void> YuGiOhBandaiSetCatalogService::save(const YuGiOhBandaiSetCatalog& catalog) {
    const auto p = catalogPath();
    auto dir = fs_.ensureDirectory(p.parent_path());
    if (!dir) return dir;
    const nlohmann::json j = catalog;
    return fs_.writeText(p, j.dump(2));
}

}  // namespace ccm
