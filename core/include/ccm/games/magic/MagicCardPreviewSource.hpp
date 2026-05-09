#pragma once

// MagicCardPreviewSource: ICardPreviewSource implementation for Magic the
// Gathering. Calls Scryfall's search endpoint at
//   https://api.scryfall.com/cards/search?q=name:"<name>" AND set:<setCode>
// and returns `data[0].image_uris.normal`. Mirrors the established
// `src/components/magic/SelectedMtgPanel.tsx::getImage`, including the
// `&` -> `and` substitution in the card name.

#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>

namespace ccm {

class MagicCardPreviewSource final : public ICardPreviewSource {
public:
    explicit MagicCardPreviewSource(IHttpClient& http);

    Result<std::string> fetchImageUrl(std::string_view name,
                                      std::string_view setId,
                                      std::string_view setNo) override;

    // Build the fully URL-encoded Scryfall search URL for the given card.
    // Exposed for unit testing and to keep encoding rules in one place.
    static std::string buildSearchUrl(std::string_view name,
                                      std::string_view setId);

    // Parse a Scryfall /cards/search response body and pull out the
    // `data[0].image_uris.normal` URL. Returns an error result when no
    // matching printing is found, when the JSON is malformed, or when the
    // entry has no top-level `image_uris` (double-faced cards expose them
    // on a face object - no fallback in this compatibility behavior either).
    static Result<std::string> parseResponse(const std::string& body);

private:
    IHttpClient& http_;
};

}  // namespace ccm
