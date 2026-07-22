#pragma once

// JapanesePokemonCardPreviewSource: TCGdex ja localId-based preview + variants.
// Image URLs use /high.png (wxImage decodes PNG/JPEG, not webp).

#include "ccm/games/pokemonjp/JapanesePokemonEnCatalog.hpp"
#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ccm {

class JapanesePokemonCardPreviewSource final : public ICardPreviewSource {
public:
    JapanesePokemonCardPreviewSource(IHttpClient& http,
                                     const JapanesePokemonEnCatalog& catalog);

    [[nodiscard]] bool supportsAutoDetectPrint() const noexcept override { return true; }

    Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view name,
                      std::string_view setId,
                      std::string_view setNo) override;
    Result<AutoDetectedPrint> detectFirstPrint(std::string_view name,
                                               std::string_view setId) override;
    Result<std::vector<AutoDetectedPrint>> detectPrintVariants(std::string_view name,
                                                               std::string_view setId) override;

    static std::string normalizeLocalId(std::string_view setNo);
    static std::string buildSetDetailUrl(std::string_view setId);
    static std::string buildCardUrl(std::string_view setId, std::string_view localId);
    static std::string imageUrlFromBase(std::string_view imageBase);

    // Parse set-detail body; optionally filter by name (EN catalog / JA) and/or localId.
    struct SetCardRow {
        std::string localId;
        std::string nameJa;
        std::string imageBase;  // empty when TCGdex has no scan
        std::string rarity;
    };

    static Result<std::vector<SetCardRow>, PreviewLookupError>
        parseSetCards(const std::string& body);

    static Result<std::string, PreviewLookupError>
        parseCardImageUrl(const std::string& body);

    static Result<std::vector<AutoDetectedPrint>>
        parsePrintVariants(const std::string& body,
                           std::string_view setId,
                           std::string_view wantedCardName,
                           const JapanesePokemonEnCatalog& catalog);

    // Catalog-only Auto-detect when TCGdex has no set detail (theme decks, etc.).
    static Result<std::vector<AutoDetectedPrint>>
        detectPrintVariantsFromCatalog(std::string_view setId,
                                       std::string_view wantedCardName,
                                       const JapanesePokemonEnCatalog& catalog);

private:
    IHttpClient& http_;
    const JapanesePokemonEnCatalog& catalog_;
};

}  // namespace ccm
