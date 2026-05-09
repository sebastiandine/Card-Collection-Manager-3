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
#include <vector>

namespace ccm {

struct AutoDetectedPrint {
    std::string setNo;
    std::string rarity;
};

// Classified error returned by ICardPreviewSource::fetchImageUrl. The kind
// drives caching policy in CardPreviewService:
//
//   NotFound  -- the upstream answered cleanly that the card has no image
//                (or no matching record at all). Safe to remember: the
//                answer will not change until the user edits the card
//                record itself, which automatically invalidates the cache
//                key. Negative-cached so subsequent selections show the
//                fallback card-back instantly without another HTTP call.
//
//   Transient -- the upstream did not answer cleanly (HTTP / network /
//                timeout failure, malformed response, parse error). The
//                record may well have an image; we just couldn't see it
//                this time. NOT cached, so the next selection retries.
//
// The `message` is opaque to the service and is forwarded to the UI as
// the existing free-form `Result<std::string>::error()` string.
struct PreviewLookupError {
    enum class Kind { NotFound, Transient };
    Kind        kind{Kind::Transient};
    std::string message;
};

class ICardPreviewSource {
public:
    virtual ~ICardPreviewSource() = default;

    // Resolve the preview image URL for a single card. `setNo` is optional
    // (empty string is fine); some game APIs (e.g. Pokemon TCG) can use it as
    // a more precise lookup key, others (Magic/Scryfall) ignore it.
    //
    // Errors carry a classification (`PreviewLookupError::Kind`) so
    // CardPreviewService can decide whether to remember the miss
    // (`NotFound`) or retry on the next call (`Transient`). See the doc
    // comment on PreviewLookupError above for the exact contract.
    virtual Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view name,
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

    // Optional listing of every distinct `(set_code, rarity)` print returned by
    // the upstream for an exact card name inside the chosen display set.
    virtual Result<std::vector<AutoDetectedPrint>>
        detectPrintVariants(std::string_view /*name*/, std::string_view /*setId*/) {
        return Result<std::vector<AutoDetectedPrint>>::err(
            "Print variant listing not supported by this game.");
    }
};

}  // namespace ccm
