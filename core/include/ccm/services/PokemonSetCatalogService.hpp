#pragma once

// PokemonSetCatalogService: load/save pokemon/set-catalog-west.json and
// pokemon/set-catalog-asia.json under the configured dataStorage path.

#include "ccm/domain/Enums.hpp"
#include "ccm/domain/PokemonSetCatalog.hpp"
#include "ccm/ports/IFileSystem.hpp"
#include "ccm/services/ConfigService.hpp"
#include "ccm/util/Result.hpp"

#include <functional>
#include <string>

namespace ccm {

class PokemonSetCatalogService {
public:
    using DirNameFn = std::function<std::string(Game)>;

    PokemonSetCatalogService(IFileSystem& fs, ConfigService& config, DirNameFn dirName);

    Result<PokemonSetCatalog> load(PokemonRegion region) const;
    Result<void>              save(PokemonRegion region, const PokemonSetCatalog& catalog);

    [[nodiscard]] bool exists(PokemonRegion region) const;

private:
    IFileSystem&   fs_;
    ConfigService& config_;
    DirNameFn      dirName_;

    [[nodiscard]] std::filesystem::path catalogPath(PokemonRegion region) const;
};

}  // namespace ccm
