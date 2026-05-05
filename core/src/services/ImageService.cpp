#include "ccm/services/ImageService.hpp"

#include "ccm/util/FsNames.hpp"

namespace ccm {

ImageService::ImageService(IImageStore& store) : store_(store) {}

std::uint8_t ImageService::nextImageIndex(const std::vector<std::string>& existingImages) {
    if (existingImages.empty()) return 0;
    const std::string& last = existingImages.back();
    // Preserve the compatibility shim for legacy image filenames that
    // contain "IMG_FRONT"/"IMG_BACK" markers - those start a fresh index.
    if (last.find("IMG_FRONT") != std::string::npos ||
        last.find("IMG_BACK")  != std::string::npos) {
        return 0;
    }
    const std::uint8_t parsed = parseIndexFromFilename(last);
    // Saturating +1 since we hand back uint8 just like the Rust version.
    return parsed == 255 ? 255 : static_cast<std::uint8_t>(parsed + 1);
}

std::string ImageService::buildTargetName(bool newEntry,
                                          std::uint32_t cardId,
                                          const std::string& setName,
                                          const std::string& cardName,
                                          std::uint8_t index) {
    const std::string set  = formatTextForFs(setName);
    const std::string card = formatTextForFs(cardName);
    if (newEntry) {
        return set + "+" + card + "+" + std::to_string(static_cast<int>(index));
    }
    return std::to_string(cardId) + "+" + set + "+" + card + "+" +
           std::to_string(static_cast<int>(index));
}

Result<std::string> ImageService::addImage(Game game,
                                           const std::filesystem::path& srcPath,
                                           bool newEntry,
                                           std::uint32_t cardId,
                                           const std::string& setName,
                                           const std::string& cardName,
                                           const std::vector<std::string>& existingImages) {
    const auto idx = nextImageIndex(existingImages);
    const auto target = buildTargetName(newEntry, cardId, setName, cardName, idx);
    return store_.copyIn(game, srcPath, target);
}

Result<void> ImageService::removeImage(Game game, const std::string& imageName) {
    return store_.remove(game, imageName);
}

std::filesystem::path ImageService::resolveImagePath(Game game,
                                                     const std::string& imageName) const {
    return store_.resolvePath(game, imageName);
}

}  // namespace ccm
