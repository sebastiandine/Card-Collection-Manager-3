#pragma once

// DigiBattle99SetSource: ISetSource for Digimon Digi-Battle (1999 English).
// digimoncard.io has no dedicated sets endpoint; we derive unique pack names
// from a bulk search.php call scoped to series=Digimon Digi-Battle Card Game.

#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>

namespace ccm {

class DigiBattle99SetSource final : public ISetSource {
public:
    static constexpr const char* kEndpoint =
        "https://digimoncard.io/api-public/search.php?"
        "series=Digimon%20Digi-Battle%20Card%20Game&limit=1000&sort=name&sortdirection=asc";

    static constexpr const char* kSeries = "Digimon Digi-Battle Card Game";

    explicit DigiBattle99SetSource(IHttpClient& http);

    Result<std::vector<Set>> fetchAll() override;

    // Pure parser exposed for unit testing without a network round-trip.
    static Result<std::vector<Set>> parseResponse(const std::string& body);

    // Stable Set.id from a pack display name (ASCII lower, non-alnum -> '-').
    static std::string slugifyPackName(std::string_view packName);

private:
    IHttpClient& http_;
};

}  // namespace ccm
