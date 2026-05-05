#pragma once

// IImageStore - manages card image files on disk inside the per-game
// `images/` subdirectory. Returns absolute paths so the UI can decode with
// whichever image library it likes (we use wxImage in the wx adapter); core
// stays free of any image decoding dependency.

#include "ccm/domain/Enums.hpp"
#include "ccm/util/Result.hpp"

#include <filesystem>
#include <string>

namespace ccm {

class IImageStore {
public:
    virtual ~IImageStore() = default;

    // Copy `srcPath` into the per-game image dir under filename `targetName`
    // (extension preserved from `srcPath`). Returns the final filename
    // (basename only, no path) on success.
    virtual Result<std::string> copyIn(Game game,
                                       const std::filesystem::path& srcPath,
                                       const std::string& targetName) = 0;

    // Delete `imageName` from the per-game image dir.
    virtual Result<void> remove(Game game, const std::string& imageName) = 0;

    // Resolve `imageName` to an absolute path inside the per-game image dir.
    virtual std::filesystem::path resolvePath(Game game, const std::string& imageName) const = 0;
};

}  // namespace ccm
