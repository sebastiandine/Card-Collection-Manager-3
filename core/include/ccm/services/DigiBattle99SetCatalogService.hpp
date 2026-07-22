#pragma once

// DigiBattle99SetCatalogService: load/save digibattle99/set-catalog.json under
// the configured dataStorage path.

#include "ccm/domain/DigiBattle99SetCatalog.hpp"
#include "ccm/domain/Enums.hpp"
#include "ccm/ports/IFileSystem.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/util/Result.hpp"

#include <functional>
#include <string>

namespace ccm {

class DigiBattle99SetCatalogService {
public:
    using DirNameFn = std::function<std::string(Game)>;

    DigiBattle99SetCatalogService(IFileSystem& fs, ConfigService& config, DirNameFn dirName);

    Result<DigiBattle99SetCatalog> load() const;
    Result<void>                   save(const DigiBattle99SetCatalog& catalog);

    [[nodiscard]] bool exists() const;

private:
    IFileSystem&   fs_;
    ConfigService& config_;
    DirNameFn      dirName_;

    [[nodiscard]] std::filesystem::path catalogPath() const;
};

}  // namespace ccm
