#pragma once

// PokemonCardPreviewSource: West Pokemon previews via TCGdex EN.
// Prefers GET /v2/en/cards/{setId}-{localId}, then filtered card search, then
// set-detail name match for auto-detect. Image URLs append /high.png (wxImage
// decodes PNG, not webp).

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

    // Strip everything after the first '/' (e.g. "4/102" -> "4").
    static std::string normalizeCollectorNumber(std::string_view setNo);

    static std::string buildCardByIdUrl(std::string_view setId, std::string_view setNo);
    static std::string buildSetDetailUrl(std::string_view setId);
    static std::string buildSearchUrl(std::string_view name,
                                      std::string_view setId,
                                      std::string_view setNo);
    static std::string imageUrlFromBase(std::string_view imageBase);

    struct SetCardRow {
        std::string localId;
        std::string name;
        std::string imageBase;
        std::string rarity;
    };

    static Result<std::vector<SetCardRow>, PreviewLookupError>
        parseSetCards(const std::string& body);

    static Result<std::string, PreviewLookupError>
        parseCardByIdResponse(const std::string& body);

    // Parse a slim TCGdex cards-array search response; prefer first hit with image.
    static Result<std::string, PreviewLookupError>
        parseSearchResponse(const std::string& body);

    static Result<std::vector<AutoDetectedPrint>>
        parsePrintVariants(const std::string& body,
                           std::string_view setId,
                           std::string_view wantedCardName);

private:
    IHttpClient& http_;
};

}  // namespace ccm
