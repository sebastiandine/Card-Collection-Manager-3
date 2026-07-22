#pragma once

// JsonSetRepository: persists vector<Set> under `<dataStorage>/<dirName>/`.
// Most games use `sets.json`. Pokemon West/Asia share dir `pokemon` with
// `sets-west.json` / `sets-asia.json` (migrate-on-load from legacy paths).

#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IFileSystem.hpp"
#include "ccm/ports/ISetRepository.hpp"
#include "ccm/services/ConfigService.hpp"

#include <functional>
#include <string>

namespace ccm {

class JsonSetRepository final : public ISetRepository {
public:
    using DirNameFn = std::function<std::string(Game)>;

    JsonSetRepository(IFileSystem& fs, ConfigService& config, DirNameFn dirName);

    Result<std::vector<Set>> load(Game game) override;
    Result<void>             save(Game game, const std::vector<Set>& sets) override;

private:
    IFileSystem&    fs_;
    ConfigService&  config_;
    DirNameFn       dirName_;

    [[nodiscard]] std::filesystem::path setsPath(Game game) const;
    [[nodiscard]] std::filesystem::path legacySetsPath(Game game) const;
    [[nodiscard]] Result<std::vector<Set>> parseSetsText(const std::string& text) const;
};

}  // namespace ccm
