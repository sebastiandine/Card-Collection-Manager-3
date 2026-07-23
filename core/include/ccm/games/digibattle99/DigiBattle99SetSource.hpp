#pragma once

// DigiBattle99SetSource: ISetSource for Digimon Digi-Battle (1999 English).
// digimoncard.io has no dedicated sets endpoint; we derive unique pack names
// from a bulk search.php call scoped to series=Digimon Digi-Battle Card Game.
// The same payload also builds the set-completion catalog (parseCatalog).

#include "ccm/domain/DigiBattle99SetCatalog.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ccm {

class DigiBattle99SetSource final : public ISetSource {
public:
    static constexpr const char* kEndpoint =
        "https://digimoncard.io/api-public/search.php?"
        "series=Digimon%20Digi-Battle%20Card%20Game&limit=1000&sort=name&sortdirection=asc";

    static constexpr const char* kSeries = "Digimon Digi-Battle Card Game";

    struct FetchWithCatalog {
        std::vector<Set>         sets;
        DigiBattle99SetCatalog   catalog;
    };

    explicit DigiBattle99SetSource(IHttpClient& http);

    Result<std::vector<Set>> fetchAll() override;

    // One HTTP round-trip producing both the set list and the pack catalog.
    Result<FetchWithCatalog> fetchAllWithCatalog();

    // Pure parsers exposed for unit testing without a network round-trip.
    static Result<std::vector<Set>> parseResponse(const std::string& body);
    static Result<DigiBattle99SetCatalog> parseCatalog(const std::string& body);

    // Stable Set.id from a pack display name (ASCII lower, non-alnum -> '-').
    static std::string slugifyPackName(std::string_view packName);

private:
    IHttpClient& http_;
};

}  // namespace ccm
