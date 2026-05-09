#pragma once

// ICardPreviewSource - resolves the preview image URL for a single card via
// the active game's external API. Implementations stay HTTP-bound, no UI deps.
//
// Modeled after per-game `getImage` helpers in
// `src/components/{magic,pokemon}/Selected*Panel.tsx`. The resulting URL is
// then fetched as raw image bytes by `CardPreviewService` and decoded by the
// UI layer (wxImage in our wx adapter).

#include "ccm/util/Result.hpp"

#include <string>
#include <string_view>

namespace ccm {

struct AutoDetectedPrint {
    std::string setNo;
    std::string rarity;
};

class ICardPreviewSource {
public:
    virtual ~ICardPreviewSource() = default;

    // Resolve the preview image URL for a single card. `setNo` is optional
    // (empty string is fine); some game APIs (e.g. Pokemon TCG) can use it as
    // a more precise lookup key, others (Magic/Scryfall) ignore it.
    virtual Result<std::string> fetchImageUrl(std::string_view name,
                                              std::string_view setId,
                                              std::string_view setNo) = 0;

    // Opt-in switch for per-game print metadata detection.
    [[nodiscard]] virtual bool supportsAutoDetectPrint() const noexcept { return false; }

    // Optional metadata lookup used by game-specific edit dialogs. The default
    // implementation returns an explicit "unsupported" error so games without
    // print metadata APIs do not need to override it.
    virtual Result<AutoDetectedPrint> detectFirstPrint(std::string_view /*name*/,
                                                       std::string_view /*setId*/) {
        return Result<AutoDetectedPrint>::err("Auto-detect not supported by this game.");
    }
};

}  // namespace ccm
