#pragma once

// YuGiOhSetSource: ISetSource implementation for Yu-Gi-Oh via YGOPRODeck.

#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

namespace ccm {

class YuGiOhSetSource final : public ISetSource {
public:
    static constexpr const char* kEndpoint = "https://db.ygoprodeck.com/api/v7/cardsets.php";

    explicit YuGiOhSetSource(IHttpClient& http);

    Result<std::vector<Set>> fetchAll() override;
    static Result<std::vector<Set>> parseResponse(const std::string& body);

private:
    IHttpClient& http_;
};

}  // namespace ccm
