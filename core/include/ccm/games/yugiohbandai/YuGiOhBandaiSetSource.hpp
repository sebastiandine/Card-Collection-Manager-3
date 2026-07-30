#pragma once

// YuGiOhBandaiSetSource: hardcoded Bandai set manifest + Yugipedia gallery
// wikitext catalogs for set completion.

#include "ccm/domain/Set.hpp"
#include "ccm/domain/YuGiOhBandaiSetCatalog.hpp"
#include "ccm/games/IGameModule.hpp"
#include "ccm/ports/IHttpClient.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ccm {

class YuGiOhBandaiSetSource final : public ISetSource {
public:
    struct FetchWithCatalog {
        std::vector<Set>         sets;
        YuGiOhBandaiSetCatalog   catalog;
    };

    struct SetManifestEntry {
        const char* id;
        const char* name;
        const char* releaseDate;  // YYYY/MM/DD
        const char* galleryPage;  // Yugipedia page title (may be shared)
        // For the shared promo gallery: keep cards whose setNo starts with
        // this prefix (empty = keep all from that page into this pack).
        const char* setNoPrefix;
    };

    explicit YuGiOhBandaiSetSource(IHttpClient& http);

    Result<std::vector<Set>> fetchAll() override;

    Result<FetchWithCatalog> fetchAllWithCatalog();

    [[nodiscard]] static const std::vector<SetManifestEntry>& setManifest();

    static Result<std::vector<Set>> parseResponse(const std::string& /*unused*/);

    // Parse one gallery wikitext body into checklist cards.
    static Result<std::vector<YuGiOhBandaiCatalogCard>>
    parseGalleryWikitext(const std::string& wikitext);

    // Map a Bandai number string to a set id (ban1/ban2/ban3/promos/sealdass).
    static std::string setIdForNumber(std::string_view setNo);

    static std::string setNameForId(std::string_view setId);

    // Normalize printed numbers: strip leading zeros on pure-decimal values;
    // uppercase letter prefixes (j1 → J1). Sealdass stays unpadded decimal.
    static std::string normalizeCardNumber(std::string_view setNo);

    static std::string expandRarityCode(std::string_view code);

    static std::string buildGalleryParseUrl(std::string_view pageTitle);

    static std::string englishNameFromGalleryTitle(std::string_view pageTitle);

private:
    IHttpClient& http_;
};

}  // namespace ccm
