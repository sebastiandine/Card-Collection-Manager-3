#pragma once

// YuGiOhBandaiSetCatalogService: load/save yugiohbandai/set-catalog.json.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/YuGiOhBandaiSetCatalog.hpp"
#include "ccm/ports/IFileSystem.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/util/Result.hpp"

#include <functional>
#include <string>

namespace ccm {

class YuGiOhBandaiSetCatalogService {
public:
    using DirNameFn = std::function<std::string(Game)>;

    YuGiOhBandaiSetCatalogService(IFileSystem& fs, ConfigService& config, DirNameFn dirName);

    Result<YuGiOhBandaiSetCatalog> load() const;
    Result<void>                   save(const YuGiOhBandaiSetCatalog& catalog);

    [[nodiscard]] bool exists() const;

private:
    IFileSystem&   fs_;
    ConfigService& config_;
    DirNameFn      dirName_;

    [[nodiscard]] std::filesystem::path catalogPath() const;
};

}  // namespace ccm
