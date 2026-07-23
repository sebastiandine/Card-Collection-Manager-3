#pragma once

// YuGiOhSetSource: ISetSource implementation for Yu-Gi-Oh via YGOPRODeck.
// Sets come from cardsets.php; the set-completion catalog is built from the
// unfiltered cardinfo.php dump (card_sets[] per card).

#include "ccm/domain/Set.hpp"
#include "ccm/domain/YuGiOhSetCatalog.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <vector>

namespace ccm {

class YuGiOhSetSource final : public ISetSource {
public:
    static constexpr const char* kEndpoint = "https://db.ygoprodeck.com/api/v7/cardsets.php";
    static constexpr const char* kCardInfoEndpoint =
        "https://db.ygoprodeck.com/api/v7/cardinfo.php";

    struct FetchWithCatalog {
        std::vector<Set>   sets;
        YuGiOhSetCatalog   catalog;
    };

    explicit YuGiOhSetSource(IHttpClient& http);

    Result<std::vector<Set>> fetchAll() override;

    // Two HTTP round-trips: cardsets.php for the set list, cardinfo.php for
    // the pack checklist catalog.
    Result<FetchWithCatalog> fetchAllWithCatalog();

    static Result<std::vector<Set>> parseResponse(const std::string& body);

    // Build the offline checklist from a cardinfo.php body, resolving pack
    // ids against the already-parsed sets list (by set_name → Set.id).
    static Result<YuGiOhSetCatalog> parseCatalog(const std::string&          body,
                                                 const std::vector<Set>&     sets);

private:
    IHttpClient& http_;
};

}  // namespace ccm
