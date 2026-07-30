#pragma once

// YuGiOhBandaiSetCatalog: offline pack → card checklist for Bandai set
// completion. Filled from Yugipedia set-gallery wikitext and persisted at
// `<dataStorage>/yugiohbandai/set-catalog.json`.

#include <nlohmann/json.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace ccm {

struct YuGiOhBandaiCatalogCard {
    std::string setNo;
    std::string name;
    std::string rarity;

    friend bool operator==(const YuGiOhBandaiCatalogCard&,
                           const YuGiOhBandaiCatalogCard&) = default;
};

struct YuGiOhBandaiSetCatalogPack {
    std::string setId;
    std::string setName;
    std::vector<YuGiOhBandaiCatalogCard> cards;

    friend bool operator==(const YuGiOhBandaiSetCatalogPack&,
                           const YuGiOhBandaiSetCatalogPack&) = default;
};

struct YuGiOhBandaiSetCatalog {
    std::vector<YuGiOhBandaiSetCatalogPack> packs;

    [[nodiscard]] const YuGiOhBandaiSetCatalogPack* findPack(
        std::string_view setId) const;

    [[nodiscard]] bool empty() const noexcept { return packs.empty(); }

    friend bool operator==(const YuGiOhBandaiSetCatalog&,
                           const YuGiOhBandaiSetCatalog&) = default;
};

void to_json(nlohmann::json& j, const YuGiOhBandaiCatalogCard& c);
void from_json(const nlohmann::json& j, YuGiOhBandaiCatalogCard& c);
void to_json(nlohmann::json& j, const YuGiOhBandaiSetCatalogPack& p);
void from_json(const nlohmann::json& j, YuGiOhBandaiSetCatalogPack& p);
void to_json(nlohmann::json& j, const YuGiOhBandaiSetCatalog& c);
void from_json(const nlohmann::json& j, YuGiOhBandaiSetCatalog& c);

}  // namespace ccm
