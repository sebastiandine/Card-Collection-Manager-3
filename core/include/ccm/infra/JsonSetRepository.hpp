#pragma once

// JsonSetRepository: persists vector<Set> to `<dataStorage>/<game>/sets.json`.

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
};

}  // namespace ccm
