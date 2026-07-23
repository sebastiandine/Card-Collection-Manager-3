#pragma once

// PokemonSetSource: ISetSource implementation for the Pokemon TCG.
// Calls the Pokemon TCG API at https://api.pokemontcg.io/v2/sets, maps the
// response into our `Set` domain type, and sorts by release date ascending.
// The Pokemon TCG API already returns `releaseDate` in `YYYY/MM/DD` format,
// so no rewriting is needed (unlike Scryfall's `released_at`).
// Behavior matches `pokemon/set_services.rs::update_sets`.
// Set-completion catalog is built from a paginated /v2/cards dump.

#include "ccm/domain/PokemonSetCatalog.hpp"
#include "ccm/domain/Set.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <vector>

namespace ccm {

class PokemonSetSource final : public ISetSource {
public:
    static constexpr const char* kEndpoint = "https://api.pokemontcg.io/v2/sets";
    static constexpr const char* kCardsEndpoint = "https://api.pokemontcg.io/v2/cards";
    static constexpr int         kCardsPageSize = 250;

    struct FetchWithCatalog {
        std::vector<Set>   sets;
        PokemonSetCatalog  catalog;
    };

    explicit PokemonSetSource(IHttpClient& http);

    Result<std::vector<Set>> fetchAll() override;

    // Sets endpoint + paginated cards dump for the offline checklist.
    Result<FetchWithCatalog> fetchAllWithCatalog();

    // Pure parser exposed for unit testing without a network round-trip.
    static Result<std::vector<Set>> parseResponse(const std::string& body);

    // Build / merge checklist packs from one /v2/cards page body. Pass an
    // accumulating catalog; returns page count metadata for pagination.
    struct CardsPageMeta {
        int page{1};
        int pageSize{kCardsPageSize};
        int count{0};
        int totalCount{0};
    };
    static Result<CardsPageMeta> mergeCardsPage(const std::string& body,
                                                PokemonSetCatalog& catalog,
                                                const std::vector<Set>& sets);

    static Result<PokemonSetCatalog> parseCatalog(const std::string& body,
                                                  const std::vector<Set>& sets);

    static std::string buildCardsPageUrl(int page, int pageSize = kCardsPageSize);

private:
    IHttpClient& http_;
};

}  // namespace ccm
