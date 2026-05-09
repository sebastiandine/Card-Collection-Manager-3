#include "ccm/infra/LocalImageStore.hpp"

#include <utility>

namespace ccm {

namespace fs = std::filesystem;

LocalImageStore::LocalImageStore(IFileSystem& fs, ConfigService& config, DirNameFn dirName)
    : fs_(fs), config_(config), dirName_(std::move(dirName)) {}

fs::path LocalImageStore::gameImageDir(Game game) const {
    return fs::path(config_.current().dataStorage) / dirName_(game) / "images";
}

Result<std::string> LocalImageStore::copyIn(Game game,
                                            const fs::path& srcPath,
                                            const std::string& targetName) {
    const auto dir = gameImageDir(game);
    auto ensure = fs_.ensureDirectory(dir);
    if (!ensure) return Result<std::string>::err(ensure.error());

    // Preserve the source's extension - the original Rust code does the same.
    std::string ext = srcPath.extension().string();
    std::string finalName = targetName + ext;
    const auto dest = dir / finalName;

    auto cp = fs_.copyFile(srcPath, dest, /*overwrite=*/true);
    if (!cp) return Result<std::string>::err(cp.error());
    return Result<std::string>::ok(std::move(finalName));
}

Result<void> LocalImageStore::remove(Game game, const std::string& imageName) {
    const auto p = gameImageDir(game) / imageName;
    if (!fs_.exists(p)) return Result<void>::ok();  // be forgiving on stale entries
    return fs_.remove(p);
}

fs::path LocalImageStore::resolvePath(Game game, const std::string& imageName) const {
    return gameImageDir(game) / imageName;
}

}  // namespace ccm
