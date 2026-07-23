#pragma once

// PokemonSetSource: ISetSource for West Pokemon via TCGdex EN
// (https://api.tcgdex.net/v2/en). List endpoint returns a slim array; release
// dates and set-completion checklists come from per-set detail GETs.

#include "ccm/domain/PokemonSetCatalog.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ccm {

class PokemonSetSource final : public ISetSource {
public:
    static constexpr const char* kListEndpoint = "https://api.tcgdex.net/v2/en/sets";

    struct FetchWithCatalog {
        std::vector<Set>   sets;
        PokemonSetCatalog  catalog;
    };

    explicit PokemonSetSource(IHttpClient& http);

    Result<std::vector<Set>> fetchAll() override;

    // List + per-set detail (cards + release date) for the offline checklist.
    Result<FetchWithCatalog> fetchAllWithCatalog();

    // Pure parsers exposed for unit testing without a network round-trip.
    static Result<std::vector<Set>> parseListResponse(const std::string& body);
    static Result<std::string> parseReleaseDate(const std::string& detailBody);
    static std::string rewriteReleaseDate(std::string_view isoDate);
    static std::string buildSetDetailUrl(std::string_view setId);

    static Result<PokemonSetCatalogPack> parseCatalogPackFromSetDetail(
        const std::string& detailBody,
        const Set&         set);

private:
    IHttpClient& http_;
};

}  // namespace ccm
