#pragma once

// PokemonCardPreviewSource: ICardPreviewSource implementation for the Pokemon
// TCG. Calls the Pokemon TCG search endpoint at
//   https://api.pokemontcg.io/v2/cards?q=name:"<name>" set.id:<setId> number:<setNo>
// and returns `data[0].images.large` (with `images.small` as a graceful
// fallback). Mirrors the established `getImage` flow in
// `src/components/pokemon/SelectedPokemonPanel.tsx`.

#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>

namespace ccm {

class PokemonCardPreviewSource final : public ICardPreviewSource {
public:
    explicit PokemonCardPreviewSource(IHttpClient& http);

    Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view name,
                      std::string_view setId,
                      std::string_view setNo) override;

    // Build the fully URL-encoded Pokemon TCG search URL for the given card.
    // Exposed for unit testing and to keep encoding rules in one place.
    static std::string buildSearchUrl(std::string_view name,
                                      std::string_view setId,
                                      std::string_view setNo);

    // Parse a Pokemon TCG /v2/cards response body and pull out the image URL
    // for the first matching card. Prefers `images.large`, falls back to
    // `images.small`. Errors are classified:
    //   - JSON parse failure or missing/non-array `data` => Transient.
    //   - Empty `data` array or missing image variants => NotFound.
    static Result<std::string, PreviewLookupError>
        parseResponse(const std::string& body);

private:
    IHttpClient& http_;
};

}  // namespace ccm
