#pragma once

#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>

namespace ccm {

class YuGiOhCardPreviewSource final : public ICardPreviewSource {
public:
    explicit YuGiOhCardPreviewSource(IHttpClient& http);
    [[nodiscard]] bool supportsAutoDetectPrint() const noexcept override { return true; }

    Result<std::string> fetchImageUrl(std::string_view name,
                                      std::string_view setId,
                                      std::string_view setNo) override;
    Result<AutoDetectedPrint> detectFirstPrint(std::string_view name,
                                               std::string_view setId) override;

    static std::string buildSearchUrl(std::string_view name, std::string_view setName);
    static Result<std::string> parseResponse(const std::string& body,
                                             std::string_view setNo,
                                             std::string_view setRarity);
    static Result<AutoDetectedPrint> parseFirstPrint(const std::string& body,
                                                     std::string_view preferredSetName);

private:
    IHttpClient& http_;
};

}  // namespace ccm
