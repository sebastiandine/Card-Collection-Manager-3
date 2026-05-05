#pragma once

// CardPreviewService: high-level operation for resolving and downloading a
// card's preview image from the active game's external API. Mirrors the
// `EntryPanelTemplate.tsx::useEffect([entry])` flow:
//   1. Per-game `ICardPreviewSource` resolves the card -> an image URL.
//   2. Service issues a GET against that URL through `IHttpClient`.
//   3. Raw bytes are returned to the caller (UI decodes them with whichever
//      image lib it prefers - we use wxImage in the wx adapter).
//
// Like `SetService`, sources are registered per `Game`. Unregistered games
// produce an explicit error result rather than silently doing nothing.

#include "ccm/domain/Enums.hpp"
#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"
#include "ccm/util/Result.hpp"

#include <string>
#include <string_view>
#include <unordered_map>

namespace ccm {

class CardPreviewService {
public:
    explicit CardPreviewService(IHttpClient& http);

    // Register a per-game preview source. The pointer-by-reference kept as a
    // raw pointer must remain valid for the lifetime of the service.
    void registerSource(Game game, ICardPreviewSource& source);

    // Resolve and download the preview image bytes for a single card.
    // The returned `std::string` is a raw byte buffer (PNG/JPEG payload) -
    // it is NOT decoded text. Use std::string::data()/size() with whatever
    // image-decoding facility your UI provides.
    Result<std::string> fetchPreviewBytes(Game game,
                                          std::string_view name,
                                          std::string_view setId,
                                          std::string_view setNo);

private:
    IHttpClient& http_;
    std::unordered_map<Game, ICardPreviewSource*> sources_;
};

}  // namespace ccm
