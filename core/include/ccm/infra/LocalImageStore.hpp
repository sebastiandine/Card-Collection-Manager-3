#pragma once

// LocalImageStore: stores card images under
// `<dataStorage>/<game>/images/<filename>`.
// Implements IImageStore, used by ImageService.

#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IFileSystem.hpp"
#include "ccm/ports/IImageStore.hpp"
#include "ccm/services/ConfigService.hpp"

#include <functional>
#include <string>

namespace ccm {

class LocalImageStore final : public IImageStore {
public:
    using DirNameFn = std::function<std::string(Game)>;

    LocalImageStore(IFileSystem& fs, ConfigService& config, DirNameFn dirName);

    Result<std::string> copyIn(Game game,
                               const std::filesystem::path& srcPath,
                               const std::string& targetName) override;
    Result<void>        remove(Game game, const std::string& imageName) override;
    std::filesystem::path resolvePath(Game game, const std::string& imageName) const override;

private:
    IFileSystem&    fs_;
    ConfigService&  config_;
    DirNameFn       dirName_;

    [[nodiscard]] std::filesystem::path gameImageDir(Game game) const;
};

}  // namespace ccm
