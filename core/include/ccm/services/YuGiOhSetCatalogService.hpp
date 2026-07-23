#pragma once

// YuGiOhSetCatalogService: load/save yugioh/set-catalog.json under the
// configured dataStorage path.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/YuGiOhSetCatalog.hpp"
#include "ccm/ports/IFileSystem.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/util/Result.hpp"

#include <functional>
#include <string>

namespace ccm {

class YuGiOhSetCatalogService {
public:
    using DirNameFn = std::function<std::string(Game)>;

    YuGiOhSetCatalogService(IFileSystem& fs, ConfigService& config, DirNameFn dirName);

    Result<YuGiOhSetCatalog> load() const;
    Result<void>             save(const YuGiOhSetCatalog& catalog);

    [[nodiscard]] bool exists() const;

private:
    IFileSystem&   fs_;
    ConfigService& config_;
    DirNameFn      dirName_;

    [[nodiscard]] std::filesystem::path catalogPath() const;
};

}  // namespace ccm
