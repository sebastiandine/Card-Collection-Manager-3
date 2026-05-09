#pragma once

#include "ccm/ports/ICardPreviewSource.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>

namespace ccm {

class YuGiOhCardPreviewSource final : public ICardPreviewSource {
public:
    explicit YuGiOhCardPreviewSource(IHttpClient& http);

    Result<std::string> fetchImageUrl(std::string_view name,
                                      std::string_view setId,
                                      std::string_view setNo) override;

    static std::string buildSearchUrl(std::string_view name, std::string_view setName);
    static Result<std::string> parseResponse(const std::string& body,
                                             std::string_view setNo,
                                             std::string_view setRarity);

private:
    IHttpClient& http_;
};

}  // namespace ccm
