#pragma once

// ImageService: applies the established image filename rules and delegates the actual
// disk operations to an IImageStore.
//
// Filename rule (mirrors Rust `magic/card_services.rs` and
// `pokemon/card_services.rs`):
//   - new entry  ->  "{set}+{name}+{idx}.{ext}"
//   - existing   ->  "{id}+{set}+{name}+{idx}.{ext}"
//
// The "+name+set+name+" convention is preserved byte-for-byte so legacy
// collections continue to display correctly when imported.

#include "ccm/domain/Enums.hpp"
#include "ccm/ports/IImageStore.hpp"
#include "ccm/util/Result.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ccm {

class ImageService {
public:
    explicit ImageService(IImageStore& store);

    // Compute the next image index from the previously stored image filenames
    // for a card. Implements the same logic as the Rust card_services modules.
    static std::uint8_t nextImageIndex(const std::vector<std::string>& existingImages);

    // Build the target filename (without extension) for a new image attached
    // to a card. Pass the card's existing id (0 if not yet stored) and a
    // `newEntry` flag matching the established semantic.
    static std::string buildTargetName(bool newEntry,
                                       std::uint32_t cardId,
                                       const std::string& setName,
                                       const std::string& cardName,
                                       std::uint8_t index);

    // Convenience: build target name + delegate copy to the IImageStore.
    Result<std::string> addImage(Game game,
                                 const std::filesystem::path& srcPath,
                                 bool newEntry,
                                 std::uint32_t cardId,
                                 const std::string& setName,
                                 const std::string& cardName,
                                 const std::vector<std::string>& existingImages);

    Result<void> removeImage(Game game, const std::string& imageName);

    [[nodiscard]] std::filesystem::path resolveImagePath(Game game,
                                                         const std::string& imageName) const;

private:
    IImageStore& store_;
};

}  // namespace ccm
