#pragma once

// YuGiOhBandaiCardPreviewSource: Yugipedia pageimages + SMW ask for Bandai
// Carddass previews and auto-detect (by English name or Bandai number).

#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ccm {

class YuGiOhBandaiCardPreviewSource final : public ICardPreviewSource {
public:
    explicit YuGiOhBandaiCardPreviewSource(IHttpClient& http);

    [[nodiscard]] bool supportsAutoDetectPrint() const noexcept override { return true; }

    Result<std::string, PreviewLookupError>
        fetchImageUrl(std::string_view name,
                      std::string_view setId,
                      std::string_view setNo) override;

    Result<AutoDetectedPrint> detectFirstPrint(std::string_view name,
                                               std::string_view setId) override;

    Result<std::vector<AutoDetectedPrint>> detectPrintVariants(std::string_view name,
                                                               std::string_view setId) override;

    Result<AutoDetectedPrint> detectBySetNo(std::string_view setId,
                                            std::string_view setNo) override;

    Result<std::vector<AutoDetectedPrint>> detectVariantsBySetNo(
        std::string_view setId,
        std::string_view setNo) override;

    // Prefer "<Name> (Bandai)" / English / Sealdass page depending on setId.
    static std::string preferredPageTitle(std::string_view name,
                                          std::string_view setId,
                                          std::string_view setNo);

    static std::string buildPageImagesUrl(std::string_view pageTitle);

    static std::string buildAskByNameUrl(std::string_view englishName);

    static std::string buildAskByNumberUrl(std::string_view setNo);

    // True for Jump/Toei promo codes (J1, TA2, …). Yugipedia's SMW
    // `Bandai number` property is numeric-only, so these must use the
    // promotional gallery instead of `action=ask`.
    [[nodiscard]] static bool isAlphanumericPromoNumber(std::string_view setNo);

    static Result<std::vector<AutoDetectedPrint>>
        parsePromoGalleryResponse(const std::string& body,
                                  std::string_view wantedSetNo);

    static Result<std::string, PreviewLookupError>
        parsePageImagesResponse(const std::string& body);

    static Result<std::vector<AutoDetectedPrint>>
        parseAskResponse(const std::string& body,
                         std::string_view preferredSetId,
                         std::string_view wantedSetNo = {});

    static AutoDetectedPrint enrichPrint(AutoDetectedPrint print,
                                         std::string_view pageTitle);

private:
    Result<std::string, PreviewLookupError> fetchPageImage(std::string_view pageTitle);

    Result<std::vector<AutoDetectedPrint>> askByName(std::string_view name,
                                                     std::string_view setId);

    Result<std::vector<AutoDetectedPrint>> askByNumber(std::string_view setId,
                                                       std::string_view setNo);

    IHttpClient& http_;
};

}  // namespace ccm
