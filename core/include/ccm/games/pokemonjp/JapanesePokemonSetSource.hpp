#pragma once

// JapanesePokemonSetSource: TCGdex ja set list + per-set detail for release
// dates. English display names come from JapanesePokemonEnCatalog when present.

#include "ccm/games/IGameModule.hpp"
#include "ccm/games/pokemonjp/JapanesePokemonEnCatalog.hpp"
#include "ccm/ports/IHttpClient.hpp"

namespace ccm {

class JapanesePokemonSetSource final : public ISetSource {
public:
    static constexpr const char* kListEndpoint = "https://api.tcgdex.net/v2/ja/sets";

    JapanesePokemonSetSource(IHttpClient& http, const JapanesePokemonEnCatalog& catalog);

    Result<std::vector<Set>> fetchAll() override;

    void augmentCachedSets(std::vector<Set>& sets) const override;

    // Pure parsers for hermetic tests.
    static Result<std::vector<Set>> parseListResponse(const std::string& body);
    static Result<std::string> parseReleaseDate(const std::string& detailBody);
    static bool shouldExcludeSetId(std::string_view setId) noexcept;
    static std::string applySetNameOverride(std::string_view setId,
                                            std::string nameJa);
    static std::string rewriteReleaseDate(std::string_view isoDate);
    static std::string buildSetDetailUrl(std::string_view setId);

    // Original-era theme decks / sheets omitted by TCGdex JA. Idempotent by id.
    static void appendMissingClassicProducts(std::vector<Set>& sets);

private:
    IHttpClient& http_;
    const JapanesePokemonEnCatalog& catalog_;
};

}  // namespace ccm
