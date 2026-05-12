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
#include <vector>

namespace ccm {

class PokemonCardPreviewSource final : public ICardPreviewSource {
public:
    explicit PokemonCardPreviewSource(IHttpClient& http);

    [[nodiscard]] bool supportsAutoDetectPrint() const noexcept override { return true; }

    Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view name,
                      std::string_view setId,
                      std::string_view setNo) override;
    Result<AutoDetectedPrint> detectFirstPrint(std::string_view name,
                                               std::string_view setId) override;
    Result<std::vector<AutoDetectedPrint>> detectPrintVariants(std::string_view name,
                                                               std::string_view setId) override;

    // Build the fully URL-encoded Pokemon TCG search URL for the given card.
    // Exposed for unit testing and to keep encoding rules in one place.
    static std::string buildSearchUrl(std::string_view name,
                                      std::string_view setId,
                                      std::string_view setNo);

    // Slimmer search URL for auto-detect: omits the number clause and asks the
    // API for only the fields the print-variant parser needs.
    static std::string buildDetectSearchUrl(std::string_view name,
                                            std::string_view setId);

    // Parse a Pokemon TCG /v2/cards response body and pull out the image URL
    // for the first matching card. Prefers `images.large`, falls back to
    // `images.small`. Errors are classified:
    //   - JSON parse failure or missing/non-array `data` => Transient.
    //   - Empty `data` array or missing image variants => NotFound.
    static Result<std::string, PreviewLookupError>
        parseResponse(const std::string& body);

    // Enumerate distinct collector numbers (and rarities) for an exact card
    // name inside the chosen set. Exposed for unit testing without HTTP.
    static Result<std::vector<AutoDetectedPrint>>
        parsePrintVariants(const std::string& body,
                           std::string_view setId,
                           std::string_view wantedCardName);

private:
    IHttpClient& http_;
};

}  // namespace ccm
