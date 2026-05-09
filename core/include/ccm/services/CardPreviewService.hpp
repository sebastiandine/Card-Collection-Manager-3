#pragma once

// CardPreviewService: high-level operation for resolving and downloading a
// card's preview image from the active game's external API. Mirrors the
// `EntryPanelTemplate.tsx::useEffect([entry])` flow:
//   1. Per-game `ICardPreviewSource` resolves the card -> an image URL.
//   2. Service issues a GET against that URL through `IHttpClient`.
//   3. Raw bytes are returned to the caller (UI decodes them with whichever
//      image lib it prefers - we use wxImage in the wx adapter).
//
// Sources are registered through `IGameModule::cardPreviewSource()`. Modules
// that return nullptr are silently skipped; games without a registered source
// produce an explicit error result on lookup rather than silently doing nothing.

#include "ccm/domain/Enums.hpp"
#include "ccm/games/IGameModule.hpp"
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

    // Register a game module's preview source. Calling this with a module
    // whose `cardPreviewSource()` returns nullptr is a no-op (the game has
    // no remote preview API). The module reference must remain valid for
    // the lifetime of the service.
    void registerModule(IGameModule& module);

    // Resolve and download the preview image bytes for a single card.
    // The returned `std::string` is a raw byte buffer (PNG/JPEG payload) -
    // it is NOT decoded text. Use std::string::data()/size() with whatever
    // image-decoding facility your UI provides.
    Result<std::string> fetchPreviewBytes(Game game,
                                          std::string_view name,
                                          std::string_view setId,
                                          std::string_view setNo);

    Result<AutoDetectedPrint> detectFirstPrint(Game game,
                                               std::string_view name,
                                               std::string_view setId);

    // Download image bytes from a fully-qualified URL without going through
    // per-game preview-source resolution.
    Result<std::string> fetchImageBytesByUrl(std::string_view url);

private:
    IHttpClient& http_;
    std::unordered_map<Game, ICardPreviewSource*> sources_;
};

}  // namespace ccm
