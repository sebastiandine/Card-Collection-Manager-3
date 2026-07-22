#pragma once

// JapanesePokemonEnCatalog - bundled English name layer for Japanese Pokémon.
// Loaded from assets/pokemon_jp_en_catalog.json (generated offline). Missing
// entries fall through to TCGdex Japanese names at runtime.

#include "ccm/util/Result.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ccm {

struct JapanesePokemonSetEnInfo {
    std::string nameEn;
    std::string nameJa;
    std::string releaseDate;  // YYYY/MM/DD when known; may be empty
};

struct JapanesePokemonPrintEnInfo {
    std::string setId;
    std::string localId;
    std::string nameEn;
    std::string nameJa;
    std::string nameEnSource;  // bulbapedia | species-table | manual
    // Classic JA gap-fill when TCGdex has no CDN scan (optional).
    std::string imageUrl;      // explicit HTTPS URL, preferred when set
    std::string tcgplayerId;   // TCGPlayer product id → product-images CDN
};

class JapanesePokemonEnCatalog {
public:
    [[nodiscard]] static Result<JapanesePokemonEnCatalog>
        parse(const std::string& jsonBody);

    [[nodiscard]] bool empty() const noexcept {
        return sets_.empty() && printsByKey_.empty();
    }

    [[nodiscard]] std::optional<JapanesePokemonSetEnInfo>
        findSet(std::string_view setId) const;

    [[nodiscard]] std::optional<JapanesePokemonPrintEnInfo>
        findPrint(std::string_view setId, std::string_view localId) const;

    // Case-insensitive match of nameEn or nameJa within a set.
    // Also matches qualified English titles: wanted "Mewtwo" hits
    // "Mewtwo (CoroCoro promo)" (prefix + " (").
    [[nodiscard]] std::vector<JapanesePokemonPrintEnInfo>
        findPrintsByName(std::string_view setId, std::string_view cardName) const;

    [[nodiscard]] bool hasPrintsForSet(std::string_view setId) const noexcept;

    // TCGPlayer product-image CDN URL for classic JA gap-fill.
    [[nodiscard]] static std::string tcgplayerImageUrl(std::string_view productId);

    // Prefer imageUrl; else build from tcgplayerId; else empty.
    [[nodiscard]] static std::string previewImageUrlFromPrint(
        const JapanesePokemonPrintEnInfo& print);

private:
    std::unordered_map<std::string, JapanesePokemonSetEnInfo> sets_;
    std::unordered_map<std::string, JapanesePokemonPrintEnInfo> printsByKey_;
    // setId -> print keys for name scans
    std::unordered_map<std::string, std::vector<std::string>> printKeysBySet_;
};

}  // namespace ccm
