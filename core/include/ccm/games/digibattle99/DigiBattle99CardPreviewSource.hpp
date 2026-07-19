#pragma once

// DigiBattle99CardPreviewSource: digimoncard.io search + CDN card images for
// Digimon Digi-Battle (1999 English).
//
// Preview key middle slot is Set.name (pack display name) so search.php?pack=
// works without a reverse slug map. When setNo is present, the CDN URL is
// built directly — no search round-trip.

#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ccm {

class DigiBattle99CardPreviewSource final : public ICardPreviewSource {
public:
    static constexpr const char* kSeries = "Digimon Digi-Battle Card Game";
    static constexpr const char* kImageBase =
        "https://images.digimoncard.io/images/cards/";

    explicit DigiBattle99CardPreviewSource(IHttpClient& http);

    [[nodiscard]] bool supportsAutoDetectPrint() const noexcept override { return true; }

    Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view name,
                      std::string_view setName,
                      std::string_view setNo) override;
    Result<AutoDetectedPrint> detectFirstPrint(std::string_view name,
                                               std::string_view setName) override;
    Result<std::vector<AutoDetectedPrint>> detectPrintVariants(std::string_view name,
                                                               std::string_view setName) override;

    // Uppercase the alphabetic prefix of a Digi-Battle card number (bo-88 -> BO-88).
    // Does not invent zero-padding — CDN keys match API ids literally.
    static std::string normalizeCardNumber(std::string_view setNo);

    // CDN preview URL for a normalized card id (.jpg — wxImage registers
    // JPEG/PNG only; digimoncard.io also serves .webp but we cannot decode it).
    static std::string buildImageUrl(std::string_view setNo);

    // digimoncard.io search URL: n= / pack= / series= / optional card=.
    // setName is the pack display name (Set.name), not the slug id.
    static std::string buildSearchUrl(std::string_view name,
                                      std::string_view setName,
                                      std::string_view setNo);

    // Parse a digimoncard.io search.php body into a CDN image URL for the
    // first exact name match (optional pack filter applied by the request).
    static Result<std::string, PreviewLookupError>
        parseImageUrlFromSearch(const std::string& body,
                                std::string_view wantedCardName);

    static Result<std::vector<AutoDetectedPrint>>
        parsePrintVariants(const std::string& body,
                           std::string_view setName,
                           std::string_view wantedCardName);

private:
    IHttpClient& http_;
};

}  // namespace ccm
