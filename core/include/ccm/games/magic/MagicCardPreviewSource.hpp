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

    Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view name,
                      std::string_view setId,
                      std::string_view setNo) override;

    // Build the fully URL-encoded Scryfall search URL for the given card.
    // Exposed for unit testing and to keep encoding rules in one place.
    static std::string buildSearchUrl(std::string_view name,
                                      std::string_view setId);

    // Parse a Scryfall /cards/search response body and pull out the
    // `data[0].image_uris.normal` URL. Errors are classified:
    //   - JSON parse failure or missing/non-array `data` => Transient.
    //   - Empty `data` array, missing top-level `image_uris`, or missing
    //     `image_uris.normal` => NotFound (the upstream answered, but the
    //     printing simply has no preview we can use).
    static Result<std::string, PreviewLookupError>
        parseResponse(const std::string& body);

private:
    IHttpClient& http_;
};

}  // namespace ccm
